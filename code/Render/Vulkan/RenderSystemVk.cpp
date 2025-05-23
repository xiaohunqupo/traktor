/*
 * TRAKTOR
 * Copyright (c) 2022-2025 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#if defined(__ANDROID__)
#	include <android_native_app_glue.h>
#endif
#include "Render/Vulkan/RenderSystemVk.h"

#include "Core/Log/Log.h"
#include "Core/Misc/Align.h"
#include "Core/Misc/AutoPtr.h"
#include "Core/Misc/Murmur3.h"
#include "Core/Misc/SafeDestroy.h"
#include "Core/Misc/StringSplit.h"
#include "Core/Misc/TString.h"
#include "Render/VertexElement.h"
#include "Render/Vulkan/AccelerationStructureVk.h"
#include "Render/Vulkan/BufferDynamicVk.h"
#include "Render/Vulkan/BufferStaticVk.h"
#include "Render/Vulkan/Private/ApiLoader.h"
#include "Render/Vulkan/Private/CommandBuffer.h"
#include "Render/Vulkan/Private/Context.h"
#include "Render/Vulkan/Private/PipelineLayoutCache.h"
#include "Render/Vulkan/Private/Queue.h"
#include "Render/Vulkan/Private/ShaderModuleCache.h"
#include "Render/Vulkan/Private/Utilities.h"
#include "Render/Vulkan/Private/VertexAttributes.h"
#include "Render/Vulkan/ProgramResourceVk.h"
#include "Render/Vulkan/ProgramVk.h"
#include "Render/Vulkan/RenderTargetDepthVk.h"
#include "Render/Vulkan/RenderTargetSetVk.h"
#include "Render/Vulkan/RenderTargetVk.h"
#include "Render/Vulkan/RenderViewVk.h"
#include "Render/Vulkan/TextureVk.h"
#include "Render/Vulkan/VertexLayoutVk.h"

#include <cstring>

#if defined(_WIN32)
#	include "Render/Vulkan/Win32/Window.h"
#elif defined(__LINUX__) || defined(__RPI__)
#	include "Render/Vulkan/Linux/Window.h"
#elif defined(__ANDROID__)
#	include "Core/System/Android/DelegateInstance.h"
#elif defined(__IOS__)
#	include "Render/Vulkan/iOS/Utilities.h"
#endif

// https://github.com/WilliamLewww/vulkan_ray_tracing_minimal_abstraction/blob/master/headless/src/main.cpp

namespace traktor::render
{
namespace
{

const char* c_validationLayerNames[] = { "VK_LAYER_KHRONOS_validation", nullptr };
#if defined(_WIN32)
const char* c_extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_utils", "VK_KHR_get_physical_device_properties2" };
#elif defined(__LINUX__) || defined(__RPI__)
const char* c_extensions[] = { "VK_KHR_surface", "VK_KHR_xlib_surface", "VK_EXT_debug_utils", "VK_KHR_get_physical_device_properties2" };
#elif defined(__ANDROID__)
const char* c_extensions[] = { "VK_KHR_surface", "VK_KHR_android_surface" };
#elif defined(__MAC__)
const char* c_extensions[] = { "VK_KHR_surface", "VK_EXT_metal_surface", "VK_EXT_debug_utils", "VK_KHR_get_physical_device_properties2" };
#else
const char* c_extensions[] = { "VK_KHR_surface", "VK_EXT_debug_utils", "VK_KHR_get_physical_device_properties2" };
#endif

#if defined(__ANDROID__) || defined(__RPI__)
const char* c_deviceExtensions[] = { "VK_KHR_swapchain" };
#else
const char* c_deviceExtensions[] = {
	"VK_KHR_swapchain",
	"VK_KHR_storage_buffer_storage_class", // Required by VK_KHR_16bit_storage, VK_KHR_8bit_storage and VK_KHR_shader_float16_int8.
	"VK_KHR_16bit_storage",
	"VK_KHR_8bit_storage",
	"VK_KHR_shader_non_semantic_info",
	"VK_KHR_shader_float16_int8",
	"VK_EXT_shader_subgroup_ballot",
	"VK_EXT_memory_budget",
	"VK_EXT_descriptor_indexing",
	"VK_KHR_buffer_device_address",
};
const char* c_deviceExtensionsRayTracing[] = {
	// Ray tracing
	"VK_KHR_deferred_host_operations",
	"VK_KHR_ray_tracing_pipeline",
	"VK_KHR_acceleration_structure",
	"VK_KHR_ray_query"
};
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	// #note We're ignoring VUID-VkShaderModuleCreateInfo-pCode-08737 since it's been a known bug in Vulkan
	// validation layer and are still causing issues.
	if (pCallbackData && pCallbackData->pMessage && pCallbackData->messageIdNumber != 0xa5625282)
	{
		std::wstring message = mbstows(pCallbackData->pMessage);
		std::wstring spec;

		const size_t s = message.find(L"The Vulkan spec states: ");
		if (s != message.npos)
		{
			spec = message.substr(s);
			message = message.substr(0, s);
		}

		LogStream* ls = &log::info;
		if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
			ls = &log::warning;
		if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
			ls = &log::error;

		(*ls) << L"-------------------------------------------------------------------------------" << Endl;
		for (auto s : StringSplit< std::wstring >(message, L";"))
			(*ls) << s << Endl;
		if (!spec.empty())
		{
			(*ls) << Endl;
			for (auto s : StringSplit< std::wstring >(spec, L";"))
				(*ls) << s << Endl;
		}
		(*ls) << L"-------------------------------------------------------------------------------" << Endl;
	}
	return VK_FALSE;
}

bool isDeviceSuitable(VkPhysicalDevice device, VkPhysicalDeviceType deviceType)
{
	VkPhysicalDeviceProperties dp;
	// VkPhysicalDeviceFeatures df;

	vkGetPhysicalDeviceProperties(device, &dp);
	// vkGetPhysicalDeviceFeatures(device, &df);

	if (dp.deviceType != deviceType)
		return false;

	return true;
}

}

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.render.RenderSystemVk", 0, RenderSystemVk, IRenderSystem)

bool RenderSystemVk::create(const RenderSystemDesc& desc)
{
	VkResult result;

#if defined(__LINUX__) || defined(__RPI__)
	if ((m_display = XOpenDisplay(0)) == nullptr)
	{
		log::error << L"Unable to create Vulkan renderer; Failed to open X display" << Endl;
		return false;
	}
#elif defined(__ANDROID__)
	auto app = desc.sysapp.instance->getApplication();
	m_screenWidth = ANativeWindow_getWidth(app->window);
	m_screenHeight = ANativeWindow_getHeight(app->window);
#endif

	if (!initializeVulkanApi())
	{
		log::error << L"Unable to create Vulkan renderer; Failed to initialize Vulkan API." << Endl;
		return false;
	}

	// Check for available layers.
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, 0);

	AutoArrayPtr< VkLayerProperties > layersAvailable(new VkLayerProperties[layerCount]);
	vkEnumerateInstanceLayerProperties(&layerCount, layersAvailable.ptr());

	AlignedVector< const char* > validationLayers;
	if (desc.validation)
	{
		for (uint32_t i = 0; i < layerCount; ++i)
		{
			bool found = false;
			for (uint32_t j = 0; c_validationLayerNames[j] != nullptr; ++j)
				if (strcmp(layersAvailable[i].layerName, c_validationLayerNames[j]) == 0)
					found = true;
			if (found)
				validationLayers.push_back(strdup(layersAvailable[i].layerName));
		}
		if (!validationLayers.empty())
			for (auto validationLayer : validationLayers)
				log::info << L"Using validation layer \"" << mbstows(validationLayer) << L"\"." << Endl;
		else
			log::warning << L"No validation layers found; validation disabled." << Endl;
	}

	// Create Vulkan instance.
	const VkApplicationInfo applicationInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "Traktor",
		.pEngineName = "Traktor",
		.engineVersion = 1,
		.apiVersion = VK_MAKE_VERSION(1, 2, 0)
	};

	const VkInstanceCreateInfo instanceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = (uint32_t)validationLayers.size(),
		.ppEnabledLayerNames = validationLayers.c_ptr(),
		.enabledExtensionCount = sizeof_array(c_extensions),
		.ppEnabledExtensionNames = c_extensions
	};

	if ((result = vkCreateInstance(&instanceCreateInfo, 0, &m_instance)) != VK_SUCCESS)
	{
		log::error << L"Failed to create Vulkan instance (" << getHumanResult(result) << L")." << Endl;
		return false;
	}

	// Load Vulkan extensions.
	if (!initializeVulkanExtensions(m_instance))
	{
		log::error << L"Failed to create Vulkan; failed to load extensions." << Endl;
		return false;
	}

#if !defined(__ANDROID__)
	if (desc.validation)
	{
		// Setup debug port callback.
		VkDebugUtilsMessengerCreateInfoEXT dumci = {};
		dumci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dumci.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dumci.messageType = /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT /*| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT*/;
		dumci.pfnUserCallback = debugCallback;

		if ((result = vkCreateDebugUtilsMessengerEXT(m_instance, &dumci, nullptr, &m_debugMessenger)) != VK_SUCCESS)
		{
			log::error << L"Failed to create Vulkan; failed to set debug report callback." << Endl;
			return false;
		}
	}
#endif

	// Select physical device.
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, 0);
	if (physicalDeviceCount == 0)
	{
		log::error << L"Failed to create Vulkan; no physical devices." << Endl;
		return false;
	}
	log::info << L"Found " << physicalDeviceCount << L" physical device(s)," << Endl;

	AutoArrayPtr< VkPhysicalDevice > physicalDevices(new VkPhysicalDevice[physicalDeviceCount]);
	vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.ptr());

	for (uint32_t i = 0; !m_physicalDevice && i < physicalDeviceCount; ++i)
	{
		VkPhysicalDeviceProperties pdp = {};
		vkGetPhysicalDeviceProperties(physicalDevices[i], &pdp);
		if (isDeviceSuitable(physicalDevices[i], VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU))
			m_physicalDevice = physicalDevices[i];
	}
	for (uint32_t i = 0; !m_physicalDevice && i < physicalDeviceCount; ++i)
	{
		VkPhysicalDeviceProperties pdp = {};
		vkGetPhysicalDeviceProperties(physicalDevices[i], &pdp);
		if (isDeviceSuitable(physicalDevices[i], VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))
			m_physicalDevice = physicalDevices[i];
	}
	if (!m_physicalDevice)
	{
		log::warning << L"Unable to find a suitable device; attempting to use first reported." << Endl;
		m_physicalDevice = physicalDevices[0];
	}

	// Get physical device queues.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, 0);

	AutoArrayPtr< VkQueueFamilyProperties > queueFamilyProperties(new VkQueueFamilyProperties[queueFamilyCount]);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilyProperties.ptr());

	// Select graphics queue.
	uint32_t graphicsQueueIndex = ~0;
	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
		{
			graphicsQueueIndex = i;
			break;
		}
	}
	if (graphicsQueueIndex == ~0)
	{
		log::error << L"Failed to create Vulkan; no suitable graphics queue found." << Endl;
		return false;
	}

	// Select compute queue.
	uint32_t computeQueueIndex = ~0;
	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if ((queueFamilyProperties[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == VK_QUEUE_COMPUTE_BIT)
		{
			computeQueueIndex = i;
			break;
		}
	}
	if (computeQueueIndex == ~0)
	{
		log::error << L"Failed to create Vulkan; no suitable compute queue found." << Endl;
		return false;
	}

	log::info << L"Using graphics queue " << graphicsQueueIndex << L", compute queue " << computeQueueIndex << L"." << Endl;

	// Build array of required extensions.
	AlignedVector< const char* > deviceExtensions;
	for (int32_t i = 0; i < sizeof_array(c_deviceExtensions); ++i)
		deviceExtensions.push_back(c_deviceExtensions[i]);
	if (desc.rayTracing)
		for (int32_t i = 0; i < sizeof_array(c_deviceExtensionsRayTracing); ++i)
			deviceExtensions.push_back(c_deviceExtensionsRayTracing[i]);

	// Create logical device.
	const VkPhysicalDeviceFeatures features = {
		.sampleRateShading = VK_TRUE,
		.multiDrawIndirect = VK_TRUE,
		.samplerAnisotropy = VK_TRUE,
		.shaderClipDistance = VK_TRUE
	};

	const void* headFeature = nullptr;

#if !defined(__ANDROID__) && !defined(__RPI__)
	const VkPhysicalDevice8BitStorageFeaturesKHR features8bitStorage = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
		.pNext = nullptr,
		.storageBuffer8BitAccess = VK_FALSE,
		.uniformAndStorageBuffer8BitAccess = VK_TRUE,
		.storagePushConstant8 = VK_FALSE
	};

	const VkPhysicalDeviceFloat16Int8FeaturesKHR features16bitFloat = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
		.pNext = (void*)&features8bitStorage,
		.shaderFloat16 = VK_FALSE,
		.shaderInt8 = VK_TRUE
	};

	// Bindless textures.
	const VkPhysicalDeviceDescriptorIndexingFeatures featuresDescriptorIndexing = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = (void*)&features16bitFloat,
		.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
		.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
		.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE
	};

	const VkPhysicalDeviceBufferDeviceAddressFeatures featuresBufferDeviceAddress{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.pNext = (void*)&featuresDescriptorIndexing,
		.bufferDeviceAddress = VK_TRUE,
		.bufferDeviceAddressCaptureReplay = VK_FALSE,
		.bufferDeviceAddressMultiDevice = VK_FALSE
	};

	const VkPhysicalDeviceVulkan11Features featuresVulkan1_1 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = (void*)&featuresBufferDeviceAddress,
		.storageBuffer16BitAccess = VK_TRUE,
		.uniformAndStorageBuffer16BitAccess = VK_TRUE,
		.storagePushConstant16 = VK_FALSE,
		.storageInputOutput16 = VK_FALSE,
		.multiview = VK_FALSE,
		.multiviewGeometryShader = VK_FALSE,
		.multiviewTessellationShader = VK_FALSE,
		.variablePointersStorageBuffer = VK_FALSE,
		.variablePointers = VK_FALSE,
		.protectedMemory = VK_FALSE,
		.samplerYcbcrConversion = VK_FALSE,
		.shaderDrawParameters = VK_TRUE
	};

	headFeature = &featuresVulkan1_1;

	if (desc.rayTracing)
	{
		static const VkPhysicalDeviceAccelerationStructureFeaturesKHR featuresAccelerationStructure = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.pNext = (void*)&featuresVulkan1_1,
			.accelerationStructure = VK_TRUE,
			.accelerationStructureCaptureReplay = VK_FALSE,
			.accelerationStructureIndirectBuild = VK_FALSE,
			.accelerationStructureHostCommands = VK_FALSE,
			.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
		};

		// static const VkPhysicalDeviceRayTracingPipelineFeaturesKHR featuresRayTracingPipeline =
		//{
		//	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
		//	.pNext = (void*)&featuresAccelerationStructure,
		//	.rayTracingPipeline = VK_TRUE,
		//	.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
		//	.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
		//	.rayTracingPipelineTraceRaysIndirect = VK_FALSE,
		//	.rayTraversalPrimitiveCulling = VK_FALSE
		// };

		static const VkPhysicalDeviceRayQueryFeaturesKHR featuresRayQuery = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
			//.pNext = (void*)&featuresRayTracingPipeline,
			.pNext = (void*)&featuresAccelerationStructure,
			.rayQuery = VK_TRUE
		};

		headFeature = &featuresRayQuery;
	}
#endif

	const float queuePriorities[] = { 1.0f, 1.0f };

	StaticVector< VkDeviceQueueCreateInfo, 2 > dqcis;
	dqcis.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphicsQueueIndex,
		.queueCount = 1,
		.pQueuePriorities = queuePriorities });
	if (computeQueueIndex != graphicsQueueIndex)
		dqcis.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = computeQueueIndex,
			.queueCount = 1,
			.pQueuePriorities = queuePriorities });

	const VkDeviceCreateInfo deviceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = headFeature,
		.flags = 0,
		.queueCreateInfoCount = (uint32_t)dqcis.size(),
		.pQueueCreateInfos = dqcis.c_ptr(),
		.enabledLayerCount = (uint32_t)validationLayers.size(),
		.ppEnabledLayerNames = validationLayers.c_ptr(),
		.enabledExtensionCount = (uint32_t)deviceExtensions.size(),
		.ppEnabledExtensionNames = deviceExtensions.c_ptr(),
		.pEnabledFeatures = &features
	};

	if ((result = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, 0, &m_logicalDevice)) != VK_SUCCESS)
	{
		log::error << L"Failed to create Vulkan; unable to create logical device (" << getHumanResult(result) << L")." << Endl;
		return false;
	}

	// Create memory allocator.
	const VmaVulkanFunctions memoryAllocatorFunctions = {
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
		.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
		.vkAllocateMemory = vkAllocateMemory,
		.vkFreeMemory = vkFreeMemory,
		.vkMapMemory = vkMapMemory,
		.vkUnmapMemory = vkUnmapMemory,
		.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
		.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
		.vkBindBufferMemory = vkBindBufferMemory,
		.vkBindImageMemory = vkBindImageMemory,
		.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
		.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
		.vkCreateBuffer = vkCreateBuffer,
		.vkDestroyBuffer = vkDestroyBuffer,
		.vkCreateImage = vkCreateImage,
		.vkDestroyImage = vkDestroyImage,
		.vkCmdCopyBuffer = vkCmdCopyBuffer,
		.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR,
		.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR
	};

	VmaAllocatorCreateInfo aci = {};
#if !defined(__RPI__) && !defined(__ANDROID__) && !defined(__IOS__)
	// \note Disabled for now, not clear if we need it and until we do let's leave it disabled.
	// if (memoryAllocatorFunctions.vkGetBufferMemoryRequirements2KHR != nullptr && memoryAllocatorFunctions.vkGetImageMemoryRequirements2KHR != nullptr)
	//	aci.vulkanApiVersion = VK_API_VERSION_1_2;

	aci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
#endif
	aci.physicalDevice = m_physicalDevice;
	aci.device = m_logicalDevice;
	aci.pVulkanFunctions = &memoryAllocatorFunctions;
	aci.instance = m_instance;
#if defined(__IOS__)
	aci.preferredLargeHeapBlockSize = 32 * 1024 * 1024;
#endif
	if (vmaCreateAllocator(&aci, &m_allocator) != VK_SUCCESS)
	{
		log::error << L"Failed to create Vulkan; failed to create allocator." << Endl;
		return false;
	}

	m_context = new Context(
		m_physicalDevice,
		m_logicalDevice,
		m_allocator,
		graphicsQueueIndex,
		computeQueueIndex);
	if (!m_context->create())
	{
		log::error << L"Failed to create Vulkan; failed to create context." << Endl;
		return false;
	}

	m_shaderModuleCache = new ShaderModuleCache(m_logicalDevice);
	m_pipelineLayoutCache = new PipelineLayoutCache(m_context);
	m_maxAnisotropy = desc.maxAnisotropy;
	m_mipBias = desc.mipBias;
	m_rayTracing = desc.rayTracing;

	log::info << L"Vulkan render system created successfully." << Endl;
	return true;
}

void RenderSystemVk::destroy()
{
	m_shaderModuleCache = nullptr;
	m_pipelineLayoutCache = nullptr;
	m_context = nullptr;
	finalizeVulkanApi();
}

bool RenderSystemVk::reset(const RenderSystemDesc& desc)
{
	// \todo Update mipmap bias and maximum anisotropy.
	return true;
}

void RenderSystemVk::getInformation(RenderSystemInformation& outInfo) const
{
	outInfo.dedicatedMemoryTotal = 0;
	outInfo.sharedMemoryTotal = 0;
	outInfo.dedicatedMemoryAvailable = 0;
	outInfo.sharedMemoryAvailable = 0;
}

bool RenderSystemVk::supportRayTracing() const
{
	return m_rayTracing;
}

uint32_t RenderSystemVk::getDisplayCount() const
{
#if defined(_WIN32)
	uint32_t count = 0;

	DISPLAY_DEVICE dd = {};
	dd.cb = sizeof(dd);
	while (EnumDisplayDevices(nullptr, count, &dd, 0))
		++count;

	return count;
#else
	return 1;
#endif
}

uint32_t RenderSystemVk::getDisplayModeCount(uint32_t display) const
{
#if defined(_WIN32)
	uint32_t count = 0;

	DISPLAY_DEVICE dd = {};
	dd.cb = sizeof(dd);
	EnumDisplayDevices(nullptr, display, &dd, 0);

	DEVMODE dmgl = {};
	dmgl.dmSize = sizeof(dmgl);
	while (EnumDisplaySettings(dd.DeviceName, count, &dmgl))
		++count;

	return count;
#else
	return 0;
#endif
}

DisplayMode RenderSystemVk::getDisplayMode(uint32_t display, uint32_t index) const
{
#if defined(_WIN32)
	DISPLAY_DEVICE dd = {};
	dd.cb = sizeof(dd);
	EnumDisplayDevices(nullptr, display, &dd, 0);

	DEVMODE dmgl = {};
	dmgl.dmSize = sizeof(dmgl);
	EnumDisplaySettings(dd.DeviceName, index, &dmgl);

	DisplayMode dm;
	dm.width = dmgl.dmPelsWidth;
	dm.height = dmgl.dmPelsHeight;
	dm.refreshRate = (uint16_t)dmgl.dmDisplayFrequency;
	dm.colorBits = (uint16_t)dmgl.dmBitsPerPel;
	return dm;
#else
	return DisplayMode();
#endif
}

DisplayMode RenderSystemVk::getCurrentDisplayMode(uint32_t display) const
{
	DisplayMode dm;
#if defined(_WIN32)
	DISPLAY_DEVICE dd = {};
	dd.cb = sizeof(dd);
	EnumDisplayDevices(nullptr, display, &dd, 0);

	DEVMODE dmgl = {};
	dmgl.dmSize = sizeof(dmgl);
	EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dmgl);

	dm.width = dmgl.dmPelsWidth;
	dm.height = dmgl.dmPelsHeight;
	dm.dpi = 96;
	dm.refreshRate = dmgl.dmDisplayFrequency;
	dm.colorBits = (uint16_t)dmgl.dmBitsPerPel;
#elif defined(__LINUX__) || defined(__RPI__)
	const int screen = DefaultScreen(m_display);
	const double dpih = ((double)DisplayWidth(m_display, screen)) / (((double)DisplayWidthMM(m_display, screen)) / 25.4);
	const double dpiv = ((double)DisplayHeight(m_display, screen)) / (((double)DisplayHeightMM(m_display, screen)) / 25.4);

	dm.width = DisplayWidth(m_display, screen);
	dm.height = DisplayHeight(m_display, screen);
	dm.dpi = (uint16_t)(std::max(dpih, dpiv));
	dm.refreshRate = 59.98f;
	dm.colorBits = 32;
#elif defined(__ANDROID__)
	dm.width = m_screenWidth;
	dm.height = m_screenHeight;
	dm.dpi = 96;
	dm.refreshRate = 60.0f;
	dm.colorBits = 32;
#elif defined(__IOS__)
	dm.width = getScreenWidth();
	dm.height = getScreenHeight();
	dm.dpi = 96;
	dm.refreshRate = 60.0f;
	dm.colorBits = 32;
#endif
	return dm;
}

float RenderSystemVk::getDisplayAspectRatio(uint32_t display) const
{
	return 0.0f;
}

Ref< IRenderView > RenderSystemVk::createRenderView(const RenderViewDefaultDesc& desc)
{
	Ref< RenderViewVk > renderView = new RenderViewVk(
		m_context,
		m_instance);
	if (renderView->create(desc))
		return renderView;
	else
		return nullptr;
}

Ref< IRenderView > RenderSystemVk::createRenderView(const RenderViewEmbeddedDesc& desc)
{
	Ref< RenderViewVk > renderView = new RenderViewVk(
		m_context,
		m_instance);
	if (renderView->create(desc))
		return renderView;
	else
		return nullptr;
}

Ref< Buffer > RenderSystemVk::createBuffer(uint32_t usage, uint32_t bufferSize, bool dynamic)
{
	uint32_t usageBits = 0;
	if ((usage & BuVertex) != 0)
		usageBits |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if ((usage & BuIndex) != 0)
		usageBits |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if ((usage & BuStructured) != 0)
		usageBits |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	if ((usage & BuIndirect) != 0)
		usageBits |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	if (usageBits == 0)
		return nullptr;

	if (supportRayTracing())
	{
		if ((usage & (BuVertex | BuIndex | BuStructured)) != 0)
			usageBits |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	}

	if (dynamic)
	{
		Ref< BufferDynamicVk > buffer = new BufferDynamicVk(m_context, bufferSize, m_statistics.buffers);
		if (buffer->create(usageBits, 3 * 4))
			return buffer;
	}
	else
	{
		Ref< BufferStaticVk > buffer = new BufferStaticVk(m_context, bufferSize, m_statistics.buffers);
		if (buffer->create(usageBits))
			return buffer;
	}

	return nullptr;
}

Ref< const IVertexLayout > RenderSystemVk::createVertexLayout(const AlignedVector< VertexElement >& vertexElements)
{
	VkVertexInputBindingDescription vibd = {};
	vibd.binding = 0;
	vibd.stride = getVertexSize(vertexElements);
	vibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	AlignedVector< VkVertexInputAttributeDescription > vads;
	int32_t positionElementIndex = -1;

	for (auto vertexElement : vertexElements)
	{
		auto& vad = vads.push_back();

		vad.location = VertexAttributes::getLocation(vertexElement.getDataUsage(), vertexElement.getIndex());
		vad.binding = 0;
		vad.format = c_vkVertexElementFormats[vertexElement.getDataType()];
		vad.offset = vertexElement.getOffset();

		if (vertexElement.getDataUsage() == DataUsage::Position && vertexElement.getIndex() == 0)
			positionElementIndex = int32_t(vads.size() - 1);
	}

	Murmur3 cs;
	cs.begin();
	cs.feedBuffer(vertexElements.c_ptr(), vertexElements.size() * sizeof(VertexElement));
	cs.end();

	return new VertexLayoutVk(vibd, vads, positionElementIndex, cs.get());
}

Ref< ITexture > RenderSystemVk::createSimpleTexture(const SimpleTextureCreateDesc& desc, const wchar_t* const tag)
{
	Ref< TextureVk > texture = new TextureVk(m_context, m_statistics.simpleTextures);
	if (texture->create(desc, tag))
		return texture;
	else
		return nullptr;
}

Ref< ITexture > RenderSystemVk::createCubeTexture(const CubeTextureCreateDesc& desc, const wchar_t* const tag)
{
	Ref< TextureVk > texture = new TextureVk(m_context, m_statistics.cubeTextures);
	if (texture->create(desc, tag))
		return texture;
	else
		return nullptr;
}

Ref< ITexture > RenderSystemVk::createVolumeTexture(const VolumeTextureCreateDesc& desc, const wchar_t* const tag)
{
	Ref< TextureVk > texture = new TextureVk(m_context, m_statistics.volumeTextures);
	if (texture->create(desc, tag))
		return texture;
	else
		return nullptr;
}

Ref< IRenderTargetSet > RenderSystemVk::createRenderTargetSet(const RenderTargetSetCreateDesc& desc, IRenderTargetSet* sharedDepthStencil, const wchar_t* const tag)
{
	Ref< RenderTargetSetVk > renderTargetSet = new RenderTargetSetVk(m_context, m_statistics.renderTargetSets);
	if (renderTargetSet->create(desc, sharedDepthStencil, tag))
		return renderTargetSet;
	else
		return nullptr;
}

Ref< IAccelerationStructure > RenderSystemVk::createTopLevelAccelerationStructure(uint32_t numInstances)
{
	if (m_rayTracing)
		return AccelerationStructureVk::createTopLevel(m_context, numInstances, 3 * 4);
	else
		return nullptr;
}

Ref< IAccelerationStructure > RenderSystemVk::createAccelerationStructure(const Buffer* vertexBuffer, const IVertexLayout* vertexLayout, const Buffer* indexBuffer, IndexType indexType, const AlignedVector< Primitives >& primitives, bool dynamic)
{
	if (m_rayTracing)
		return AccelerationStructureVk::createBottomLevel(m_context, vertexBuffer, vertexLayout, indexBuffer, indexType, primitives, dynamic);
	else
		return nullptr;
}

Ref< IProgram > RenderSystemVk::createProgram(const ProgramResource* programResource, const wchar_t* const tag)
{
	Ref< const ProgramResourceVk > resource = dynamic_type_cast< const ProgramResourceVk* >(programResource);
	if (!resource)
		return nullptr;

	if (resource->m_useRayTracing && !m_rayTracing)
		return nullptr;

	Ref< ProgramVk > program = new ProgramVk(m_context, m_statistics.programs);
	if (program->create(m_shaderModuleCache, m_pipelineLayoutCache, resource, m_maxAnisotropy, m_mipBias, tag))
		return program;
	else
		return nullptr;
}

void RenderSystemVk::purge()
{
	if (m_context)
		m_context->savePipelineCache();
}

void RenderSystemVk::getStatistics(RenderSystemStatistics& outStatistics) const
{
	outStatistics = m_statistics;
}

void* RenderSystemVk::getInternalHandle() const
{
	return (*((void**)(m_instance)));
}

}
