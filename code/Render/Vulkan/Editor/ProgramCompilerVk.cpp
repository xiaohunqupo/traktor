/*
 * TRAKTOR
 * Copyright (c) 2022-2025 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "Render/Vulkan/Editor/ProgramCompilerVk.h"

#include "Core/Log/Log.h"
#include "Core/Misc/Align.h"
#include "Core/Misc/Murmur3.h"
#include "Core/Misc/Split.h"
#include "Core/Misc/String.h"
#include "Core/Settings/PropertyBoolean.h"
#include "Core/Settings/PropertyGroup.h"
#include "Core/Settings/PropertyInteger.h"
#include "Core/Settings/PropertyString.h"
#include "Render/Editor/Glsl/GlslAccelerationStructure.h"
#include "Render/Editor/Glsl/GlslContext.h"
#include "Render/Editor/Glsl/GlslImage.h"
#include "Render/Editor/Glsl/GlslSampler.h"
#include "Render/Editor/Glsl/GlslStorageBuffer.h"
#include "Render/Editor/Glsl/GlslTexture.h"
#include "Render/Editor/Glsl/GlslUniformBuffer.h"
#include "Render/Editor/Shader/Nodes.h"
#include "Render/Editor/Shader/Script.h"
#include "Render/Editor/Shader/ShaderGraph.h"
#include "Render/Editor/Shader/ShaderModule.h"
#include "Render/Vulkan/Private/Context.h"
#include "Render/Vulkan/ProgramResourceVk.h"

#include <cstring>
#include <glslang/Public/ShaderLang.h>
#include <map>
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>
#include <SPIRV/GlslangToSpv.h>
#include <sstream>

namespace traktor::render
{
namespace
{

const glslang::EShTargetClientVersion c_clientVersion = glslang::EShTargetVulkan_1_2;
const glslang::EShTargetLanguageVersion c_targetSPV = glslang::EShTargetSpv_1_5;
const spv_target_env c_targetENV = SPV_ENV_VULKAN_1_2;

TBuiltInResource getDefaultBuiltInResource()
{
	TBuiltInResource bir = {};

	bir.maxLights = 32;
	bir.maxClipPlanes = 6;
	bir.maxTextureUnits = 32;
	bir.maxTextureCoords = 32;
	bir.maxVertexAttribs = 64;
	bir.maxVertexUniformComponents = 4096;
	bir.maxVaryingFloats = 64;
	bir.maxVertexTextureImageUnits = 32;
	bir.maxCombinedTextureImageUnits = 80;
	bir.maxTextureImageUnits = 32;
	bir.maxFragmentUniformComponents = 4096;
	bir.maxDrawBuffers = 32;
	bir.maxVertexUniformVectors = 128;
	bir.maxVaryingVectors = 8;
	bir.maxFragmentUniformVectors = 16;
	bir.maxVertexOutputVectors = 16;
	bir.maxFragmentInputVectors = 15;
	bir.minProgramTexelOffset = -8;
	bir.maxProgramTexelOffset = 7;
	bir.maxClipDistances = 8;
	bir.maxComputeWorkGroupCountX = 65535;
	bir.maxComputeWorkGroupCountY = 65535;
	bir.maxComputeWorkGroupCountZ = 65535;
	bir.maxComputeWorkGroupSizeX = 1024;
	bir.maxComputeWorkGroupSizeY = 1024;
	bir.maxComputeWorkGroupSizeZ = 64;
	bir.maxComputeUniformComponents = 1024;
	bir.maxComputeTextureImageUnits = 16;
	bir.maxComputeImageUniforms = 8;
	bir.maxComputeAtomicCounters = 8;
	bir.maxComputeAtomicCounterBuffers = 1;
	bir.maxVaryingComponents = 60;
	bir.maxVertexOutputComponents = 64;
	bir.maxGeometryInputComponents = 64;
	bir.maxGeometryOutputComponents = 128;
	bir.maxFragmentInputComponents = 128;
	bir.maxImageUnits = 8;
	bir.maxCombinedImageUnitsAndFragmentOutputs = 8;
	bir.maxCombinedShaderOutputResources = 8;
	bir.maxImageSamples = 0;
	bir.maxVertexImageUniforms = 0;
	bir.maxTessControlImageUniforms = 0;
	bir.maxTessEvaluationImageUniforms = 0;
	bir.maxGeometryImageUniforms = 0;
	bir.maxFragmentImageUniforms = 8;
	bir.maxCombinedImageUniforms = 8;
	bir.maxGeometryTextureImageUnits = 16;
	bir.maxGeometryOutputVertices = 256;
	bir.maxGeometryTotalOutputComponents = 1024;
	bir.maxGeometryUniformComponents = 1024;
	bir.maxGeometryVaryingComponents = 64;
	bir.maxTessControlInputComponents = 128;
	bir.maxTessControlOutputComponents = 128;
	bir.maxTessControlTextureImageUnits = 16;
	bir.maxTessControlUniformComponents = 1024;
	bir.maxTessControlTotalOutputComponents = 4096;
	bir.maxTessEvaluationInputComponents = 128;
	bir.maxTessEvaluationOutputComponents = 128;
	bir.maxTessEvaluationTextureImageUnits = 16;
	bir.maxTessEvaluationUniformComponents = 1024;
	bir.maxTessPatchComponents = 120;
	bir.maxPatchVertices = 32;
	bir.maxTessGenLevel = 64;
	bir.maxViewports = 16;
	bir.maxVertexAtomicCounters = 0;
	bir.maxTessControlAtomicCounters = 0;
	bir.maxTessEvaluationAtomicCounters = 0;
	bir.maxGeometryAtomicCounters = 0;
	bir.maxFragmentAtomicCounters = 8;
	bir.maxCombinedAtomicCounters = 8;
	bir.maxAtomicCounterBindings = 1;
	bir.maxVertexAtomicCounterBuffers = 0;
	bir.maxTessControlAtomicCounterBuffers = 0;
	bir.maxTessEvaluationAtomicCounterBuffers = 0;
	bir.maxGeometryAtomicCounterBuffers = 0;
	bir.maxFragmentAtomicCounterBuffers = 1;
	bir.maxCombinedAtomicCounterBuffers = 1;
	bir.maxAtomicCounterBufferSize = 16384;
	bir.maxTransformFeedbackBuffers = 4;
	bir.maxTransformFeedbackInterleavedComponents = 64;
	bir.maxCullDistances = 8;
	bir.maxCombinedClipAndCullDistances = 8;
	bir.maxSamples = 4;

	bir.maxMeshOutputVerticesNV = 0;
	bir.maxMeshOutputPrimitivesNV = 0;
	bir.maxMeshWorkGroupSizeX_NV = 0;
	bir.maxMeshWorkGroupSizeY_NV = 0;
	bir.maxMeshWorkGroupSizeZ_NV = 0;
	bir.maxTaskWorkGroupSizeX_NV = 0;
	bir.maxTaskWorkGroupSizeY_NV = 0;
	bir.maxTaskWorkGroupSizeZ_NV = 0;
	bir.maxMeshViewCountNV = 0;
	bir.maxDualSourceDrawBuffersEXT = 0;

	bir.limits.nonInductiveForLoops = 1;
	bir.limits.whileLoops = 1;
	bir.limits.doWhileLoops = 1;
	bir.limits.generalUniformIndexing = 1;
	bir.limits.generalAttributeMatrixVectorIndexing = 1;
	bir.limits.generalVaryingIndexing = 1;
	bir.limits.generalSamplerIndexing = 1;
	bir.limits.generalVariableIndexing = 1;
	bir.limits.generalConstantMatrixVectorIndexing = 1;

	return bir;
}

void performOptimization(bool convertRelaxedToHalf, AlignedVector< uint32_t >& spirv)
{
	spvtools::Optimizer optimizer(c_targetENV);
	optimizer.SetMessageConsumer([](spv_message_level_t level, const char* source, const spv_position_t& position, const char* message) {
		switch (level)
		{
		case SPV_MSG_FATAL:
		case SPV_MSG_INTERNAL_ERROR:
		case SPV_MSG_ERROR:
			log::error << L"SPIRV optimization error: ";
			if (source)
				log::error << mbstows(source) << L":";
			log::error << position.line << L":" << position.column << L":" << position.index << L":";
			if (message)
				log::error << L" " << mbstows(message);
			log::error << Endl;
			break;

		case SPV_MSG_WARNING:
			log::warning << L"SPIRV optimization warning: ";
			if (source)
				log::warning << mbstows(source) << L":";
			log::warning << position.line << L":" << position.column << L":" << position.index << L":";
			if (message)
				log::warning << L" " << mbstows(message);
			log::warning << Endl;
			break;

		case SPV_MSG_INFO:
		case SPV_MSG_DEBUG:
			log::info << L"SPIRV optimization info: ";
			if (source)
				log::info << mbstows(source) << L":";
			log::info << position.line << L":" << position.column << L":" << position.index << L":";
			if (message)
				log::info << L" " << mbstows(message);
			log::info << Endl;
			break;

		default:
			break;
		}
	});

	optimizer.RegisterPerformancePasses();

	if (convertRelaxedToHalf)
		optimizer.RegisterPass(spvtools::CreateConvertRelaxedToHalfPass());

	spvtools::OptimizerOptions spvOptOptions;
	spvOptOptions.set_run_validator(false); // Validator seems to crash so we disable it for now.

	std::vector< uint32_t > opted;
	if (optimizer.Run(spirv.c_ptr(), spirv.size(), &opted, spvOptOptions))
		spirv = AlignedVector< uint32_t >(opted.begin(), opted.end());
	else
		log::warning << L"SPIR-V optimizer failed; using unoptimized IL." << Endl;
}

bool performValidation(const AlignedVector< uint32_t >& spirv)
{
	spvtools::SpirvTools st(c_targetENV);
	st.SetMessageConsumer([](
							  spv_message_level_t /* level */,
							  const char* /* source */,
							  const spv_position_t& /* position */,
							  const char* message) {
		log::error << Endl << mbstows(message) << Endl;
	});
	return st.Validate(spirv.c_ptr(), (size_t)spirv.size());
}

const std::wstring& lookupFilterName(Filter filter)
{
	const static std::wstring names[] = { L"Point", L"Linear" };
	return names[(int32_t)filter];
}

const std::wstring& lookupAddressName(Address address)
{
	const static std::wstring names[] = { L"Wrap", L"Mirror", L"Clamp", L"Border" };
	return names[(int32_t)address];
}

const std::wstring& lookupCompareFunctionName(CompareFunction compareFunction)
{
	const static std::wstring names[] = { L"Always", L"Never", L"Less", L"LessEqual", L"Greater", L"GreaterEqual", L"Equal", L"NotEqual", L"None" };
	return names[(int32_t)compareFunction];
}

}

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.render.ProgramCompilerVk", 0, ProgramCompilerVk, IProgramCompiler)

bool ProgramCompilerVk::create(IProgramCompiler* embedded)
{
	static bool s_initialized = false;
	if (!s_initialized)
	{
		if (!glslang::InitializeProcess())
			return false;
		s_initialized = true;
	}
	return true;
}

const wchar_t* ProgramCompilerVk::getRendererSignature() const
{
	return L"Vulkan";
}

Ref< ProgramResource > ProgramCompilerVk::compile(
	const ShaderGraph* shaderGraph,
	const ShaderModule* shaderModule,
	const PropertyGroup* settings,
	const std::wstring& name,
	std::list< Error >& outErrors) const
{
	RefArray< VertexOutput > vertexOutputs;
	RefArray< PixelOutput > pixelOutputs;
	RefArray< ComputeOutput > computeOutputs;
	RefArray< Script > scriptOutputs[5];

	// Gather all output nodes from shader graph, type and number
	// of output nodes determine type of shader program (vertex-,
	// pixel- or compute shader).
	for (auto node : shaderGraph->getNodes())
	{
		if (auto vertexOutput = dynamic_type_cast< VertexOutput* >(node))
			vertexOutputs.push_back(vertexOutput);
		else if (auto pixelOutput = dynamic_type_cast< PixelOutput* >(node))
			pixelOutputs.push_back(pixelOutput);
		else if (auto computeOutput = dynamic_type_cast< ComputeOutput* >(node))
			computeOutputs.push_back(computeOutput);
		else if (auto script = dynamic_type_cast< Script* >(node))
		{
			if (!script->getTechnique().empty())
			{
				if (script->getDomain() == Script::Undefined)
					return nullptr;
				scriptOutputs[(int32_t)script->getDomain()].push_back(script);
			}
		}
	}

	GlslContext cx(shaderGraph, shaderModule, settings);

	glslang::TProgram* program = new glslang::TProgram();
	glslang::TShader* vertexShader = nullptr;
	glslang::TShader* fragmentShader = nullptr;
	glslang::TShader* computeShader = nullptr;

	// Generate and compile vertex and fragment shaders.
	if (vertexOutputs.size() == 1 && pixelOutputs.size() == 1)
	{
		const auto defaultBuiltInResource = getDefaultBuiltInResource();

		// Emit shader code by traversing from output nodes.
		cx.getEmitter().emit(cx, pixelOutputs[0]);
		cx.getEmitter().emit(cx, vertexOutputs[0]);

		const std::wstring errorReport = cx.getErrorReport();
		if (!errorReport.empty())
		{
			log::error << errorReport;
			return nullptr;
		}

		const GlslRequirements vertexRequirements = cx.requirements();
		const GlslRequirements fragmentRequirements = cx.requirements();

		const auto& layout = cx.getLayout();
		const std::string vertexShaderText = wstombs(cx.getVertexShader().getGeneratedShader(settings, layout, vertexRequirements));
		const std::string fragmentShaderText = wstombs(cx.getFragmentShader().getGeneratedShader(settings, layout, fragmentRequirements));

		// Vertex shader.
		const char* vst = vertexShaderText.c_str();
		vertexShader = new glslang::TShader(EShLangVertex);
		vertexShader->setEnvClient(glslang::EShClientVulkan, c_clientVersion);
		vertexShader->setEnvTarget(glslang::EShTargetSpv, c_targetSPV);
		vertexShader->setStrings(&vst, 1);
		vertexShader->setEntryPoint("main");
		vertexShader->setSourceEntryPoint("main");
		vertexShader->setDebugInfo(true);

		const bool vertexResult = vertexShader->parse(&defaultBuiltInResource, 100, false, (EShMessages)(EShMsgVulkanRules | EShMsgSpvRules | EShMsgSuppressWarnings | EShMsgDebugInfo));
		if (vertexShader->getInfoLog())
		{
			if (!vertexResult)
				outErrors.push_back({ trim(mbstows(vertexShader->getInfoLog())),
					mbstows(vertexShaderText) });
		}
		if (!vertexResult)
			return nullptr;

		// Fragment shader.
		const char* fst = fragmentShaderText.c_str();
		fragmentShader = new glslang::TShader(EShLangFragment);
		fragmentShader->setEnvClient(glslang::EShClientVulkan, c_clientVersion);
		fragmentShader->setEnvTarget(glslang::EShTargetSpv, c_targetSPV);
		fragmentShader->setStrings(&fst, 1);
		fragmentShader->setEntryPoint("main");
		fragmentShader->setSourceEntryPoint("main");
		fragmentShader->setDebugInfo(true);

		const bool fragmentResult = fragmentShader->parse(&defaultBuiltInResource, 100, false, (EShMessages)(EShMsgVulkanRules | EShMsgSpvRules | EShMsgSuppressWarnings | EShMsgDebugInfo));
		if (fragmentShader->getInfoLog())
		{
			if (!fragmentResult)
				outErrors.push_back({ trim(mbstows(fragmentShader->getInfoLog())),
					mbstows(fragmentShaderText) });
		}
		if (!fragmentResult)
			return nullptr;

		program->addShader(vertexShader);
		program->addShader(fragmentShader);
	}
	// Generate and compile compute shader.
	if (computeOutputs.size() >= 1 || scriptOutputs[Script::Compute].size() >= 1)
	{
		const auto defaultBuiltInResource = getDefaultBuiltInResource();

		// Emit shader code by traversing from output nodes.
		for (auto computeOutput : computeOutputs)
			cx.getEmitter().emit(cx, computeOutput);
		for (auto scriptOutput : scriptOutputs[Script::Compute])
			cx.getEmitter().emit(cx, scriptOutput);

		const std::wstring errorReport = cx.getErrorReport();
		if (!errorReport.empty())
		{
			log::error << errorReport;
			return nullptr;
		}

		const GlslRequirements computeRequirements = cx.requirements();

		const auto& layout = cx.getLayout();
		const char* computeShaderText = strdup(wstombs(cx.getComputeShader().getGeneratedShader(settings, layout, computeRequirements)).c_str());

		// Compute shader.
		computeShader = new glslang::TShader(EShLangCompute);
		computeShader->setEnvClient(glslang::EShClientVulkan, c_clientVersion);
		computeShader->setEnvTarget(glslang::EShTargetSpv, c_targetSPV);
		computeShader->setStrings(&computeShaderText, 1);
		computeShader->setEntryPoint("main");
		computeShader->setSourceEntryPoint("main");
		computeShader->setDebugInfo(true);

		const bool computeResult = computeShader->parse(&defaultBuiltInResource, 100, false, (EShMessages)(EShMsgVulkanRules | EShMsgSpvRules | EShMsgSuppressWarnings | EShMsgDebugInfo));
		if (computeShader->getInfoLog())
		{
			if (!computeResult)
				outErrors.push_back({ trim(mbstows(computeShader->getInfoLog())),
					mbstows(computeShaderText) });
		}
		if (!computeResult)
			return nullptr;

		program->addShader(computeShader);
	}

	// Link shaders into a program.
	if (!program->link((EShMessages)(EShMsgSpvRules | EShMsgVulkanRules)))
		return nullptr;

	const int32_t optimize = (settings != nullptr ? settings->getProperty< int32_t >(L"Glsl.Vulkan.Optimize", 1) : 1);
	const bool validate = (settings != nullptr ? settings->getProperty< bool >(L"Glsl.Vulkan.Validate", false) : false);
	const bool convertRelaxedToHalf = (settings != nullptr ? settings->getProperty< bool >(L"Glsl.Vulkan.ConvertRelaxedToHalf", false) : false);

	// Create output resource.
	Ref< ProgramResourceVk > programResource = new ProgramResourceVk();
	programResource->m_requireRayTracing = cx.requirements().useRayTracing;
	programResource->m_name = name;
	programResource->m_renderState = cx.getRenderState();
	programResource->m_useTargetSize = cx.requirements().useTargetSize;
	programResource->m_useRayTracing = cx.requirements().useRayTracing;
	programResource->m_localWorkGroupSize[0] = cx.requirements().localSize[0];
	programResource->m_localWorkGroupSize[1] = cx.requirements().localSize[1];
	programResource->m_localWorkGroupSize[2] = cx.requirements().localSize[2];

	// Generate SPIR-V from program AST.
	glslang::SpvOptions options;
	options.generateDebugInfo = true;
	options.emitNonSemanticShaderDebugInfo = true;
	options.emitNonSemanticShaderDebugSource = true;

	auto vsi = program->getIntermediate(EShLangVertex);
	if (vsi != nullptr)
	{
		std::vector< uint32_t > vs;
		glslang::GlslangToSpv(*vsi, vs, &options);

		programResource->m_vertexShader = AlignedVector< uint32_t >(vs.begin(), vs.end());

		if (optimize > 0)
			performOptimization(convertRelaxedToHalf, programResource->m_vertexShader);

		if (validate)
		{
			if (!performValidation(programResource->m_vertexShader))
			{
				log::error << L"Validation of generated SPIR-V failed; vertex shader compile failed." << Endl;
				return nullptr;
			}
		}
	}

	auto fsi = program->getIntermediate(EShLangFragment);
	if (fsi != nullptr)
	{
		std::vector< uint32_t > fs;
		glslang::GlslangToSpv(*fsi, fs, &options);

		programResource->m_fragmentShader = AlignedVector< uint32_t >(fs.begin(), fs.end());

		if (optimize > 0)
			performOptimization(convertRelaxedToHalf, programResource->m_fragmentShader);

		if (validate)
		{
			if (!performValidation(programResource->m_fragmentShader))
			{
				log::error << L"Validation of generated SPIR-V failed; fragment shader compile failed." << Endl;
				return nullptr;
			}
		}
	}

	auto csi = program->getIntermediate(EShLangCompute);
	if (csi != nullptr)
	{
		std::vector< uint32_t > cs;
		glslang::GlslangToSpv(*csi, cs, &options);

		programResource->m_computeShader = AlignedVector< uint32_t >(cs.begin(), cs.end());

		if (optimize > 0)
			performOptimization(convertRelaxedToHalf, programResource->m_computeShader);

		if (validate)
		{
			if (!performValidation(programResource->m_computeShader))
			{
				log::error << L"Validation of generated SPIR-V failed; compute shader compile failed." << Endl;
				return nullptr;
			}
		}
	}

	// Map parameters to uniforms.
	struct ParameterMapping
	{
		int32_t ubuffer = -1;
		uint32_t ubufferOffset = 0;
		uint32_t ubufferLength = 0;
		int32_t resourceIndex = -1;
	};

	std::map< std::wstring, ParameterMapping > parameterMapping;

	for (auto resource : cx.getLayout().getBySet(GlslResource::Set::Default))
	{
		if (const auto sampler = dynamic_type_cast< const GlslSampler* >(resource))
		{
			programResource->m_samplers.push_back({ sampler->getBinding(),
				sampler->getStages(),
				sampler->getState() });
		}
		else if (const auto texture = dynamic_type_cast< const GlslTexture* >(resource))
		{
			programResource->m_textures.push_back({ texture->getName(),
				texture->getBinding(),
				texture->getStages() });

			auto& pm = parameterMapping[texture->getName()];
			pm.resourceIndex = (int32_t)programResource->m_textures.size() - 1;
		}
		else if (const auto image = dynamic_type_cast< const GlslImage* >(resource))
		{
			programResource->m_images.push_back({ image->getName(),
				image->getBinding(),
				image->getStages() });

			auto& pm = parameterMapping[image->getName()];
			pm.resourceIndex = (int32_t)programResource->m_images.size() - 1;
		}
		else if (const auto uniformBuffer = dynamic_type_cast< const GlslUniformBuffer* >(resource))
		{
			// Runtime CPU buffer index; remap from UB binding locations.
			const int32_t ubufferIndex = uniformBuffer->getBinding() - Context::NonBindlessFirstBinding;
			T_FATAL_ASSERT(ubufferIndex >= 0 && ubufferIndex <= 2);

			uint32_t size = 0;
			for (auto uniform : uniformBuffer->get())
			{
				if (uniform.length > 1)
					size = alignUp(size, 4);

				auto& pm = parameterMapping[uniform.name];
				pm.ubuffer = ubufferIndex;
				pm.ubufferOffset = size;
				pm.ubufferLength = glsl_type_width(uniform.type) * uniform.length;

				size += glsl_type_width(uniform.type) * uniform.length;
			}

			programResource->m_uniformBufferSizes[ubufferIndex] = size;
		}
		else if (const auto storageBuffer = dynamic_type_cast< const GlslStorageBuffer* >(resource))
		{
			programResource->m_sbuffers.push_back({ storageBuffer->getName(),
				storageBuffer->getBinding(),
				storageBuffer->getStages() });

			auto& pm = parameterMapping[storageBuffer->getName()];
			pm.resourceIndex = (int32_t)programResource->m_sbuffers.size() - 1;
		}
		else if (const auto accelerationStructure = dynamic_type_cast< const GlslAccelerationStructure* >(resource))
		{
			programResource->m_accelerationStructures.push_back({ accelerationStructure->getName(),
				accelerationStructure->getBinding(),
				accelerationStructure->getStages() });

			auto& pm = parameterMapping[accelerationStructure->getName()];
			pm.resourceIndex = (int32_t)programResource->m_accelerationStructures.size() - 1;
		}
	}

	for (auto p : cx.getParameters())
	{
		if (p.type <= ParameterType::Matrix) // Uniform parameter
		{
			auto it = parameterMapping.find(p.name);
			if (it == parameterMapping.end())
				return nullptr;

			const auto& pm = it->second;

			programResource->m_parameters.push_back({ p.name,
				pm.ubuffer,
				pm.ubufferOffset,
				pm.ubufferLength });
		}
		else if (p.type >= ParameterType::Texture2D && p.type <= ParameterType::TextureCube) // Texture parameter
		{
			auto it = parameterMapping.find(p.name);
			if (it == parameterMapping.end())
				return nullptr;

			const auto& pm = it->second;
			T_FATAL_ASSERT(pm.resourceIndex >= 0);

			programResource->m_parameters.push_back({ p.name,
				pm.ubuffer,
				pm.ubufferOffset,
				pm.ubufferLength,
				pm.resourceIndex,
				-1,
				-1,
				-1 });
		}
		else if (p.type == ParameterType::StructBuffer)
		{
			auto it = parameterMapping.find(p.name);
			if (it == parameterMapping.end())
				return nullptr;

			const auto& pm = it->second;
			T_FATAL_ASSERT(pm.resourceIndex >= 0);

			programResource->m_parameters.push_back({ p.name,
				pm.ubuffer,
				pm.ubufferOffset,
				pm.ubufferLength,
				-1,
				-1,
				pm.resourceIndex,
				-1 });
		}
		else if (p.type >= ParameterType::Image2D && p.type <= ParameterType::ImageCube)
		{
			auto it = parameterMapping.find(p.name);
			if (it == parameterMapping.end())
				return nullptr;

			const auto& pm = it->second;
			T_FATAL_ASSERT(pm.resourceIndex >= 0);

			programResource->m_parameters.push_back({ p.name,
				pm.ubuffer,
				pm.ubufferOffset,
				pm.ubufferLength,
				-1,
				pm.resourceIndex,
				-1,
				-1 });
		}
		else if (p.type == ParameterType::AccelerationStructure)
		{
			auto it = parameterMapping.find(p.name);
			if (it == parameterMapping.end())
				return nullptr;

			const auto& pm = it->second;
			T_FATAL_ASSERT(pm.resourceIndex >= 0);

			programResource->m_parameters.push_back({ p.name,
				pm.ubuffer,
				pm.ubufferOffset,
				pm.ubufferLength,
				-1,
				-1,
				-1,
				pm.resourceIndex });
		}
	}

	// Calculate hashes.
	{
		Murmur3 checksum;
		checksum.begin();
		checksum.feedBuffer(programResource->m_vertexShader.c_ptr(), programResource->m_vertexShader.size() * sizeof(uint32_t));
		checksum.end();
		programResource->m_vertexShaderHash = checksum.get();
	}
	{
		Murmur3 checksum;
		checksum.begin();
		checksum.feedBuffer(programResource->m_fragmentShader.c_ptr(), programResource->m_fragmentShader.size() * sizeof(uint32_t));
		checksum.end();
		programResource->m_fragmentShaderHash = checksum.get();
	}
	{
		Murmur3 checksum;
		checksum.begin();
		checksum.feedBuffer(programResource->m_computeShader.c_ptr(), programResource->m_computeShader.size() * sizeof(uint32_t));
		checksum.end();
		programResource->m_computeShaderHash = checksum.get();
	}

	{
		Murmur3 checksum;
		checksum.begin();
		checksum.feed(cx.getRenderState());
		checksum.feedBuffer(programResource->m_vertexShader.c_ptr(), programResource->m_vertexShader.size() * sizeof(uint32_t));
		checksum.feedBuffer(programResource->m_fragmentShader.c_ptr(), programResource->m_fragmentShader.size() * sizeof(uint32_t));
		checksum.feedBuffer(programResource->m_computeShader.c_ptr(), programResource->m_computeShader.size() * sizeof(uint32_t));
		checksum.end();
		programResource->m_shaderHash = checksum.get();
	}
	{
		Murmur3 checksum;
		checksum.begin();

		checksum.feed(L"PC");
		checksum.feed< uint8_t >(programResource->m_useTargetSize ? 1 : 0);
		checksum.feed< uint8_t >(programResource->m_useRayTracing ? 1 : 0);

		for (int32_t i = 0; i < 3; ++i)
		{
			if (programResource->m_uniformBufferSizes[i] > 0)
			{
				checksum.feed(L"UB");
				checksum.feed(i);
			}
		}

		checksum.feed(programResource->m_samplers.size());
		for (uint32_t i = 0; i < programResource->m_samplers.size(); ++i)
		{
			checksum.feed(L"S");
			checksum.feed(i);
			checksum.feed(programResource->m_samplers[i].binding);
			checksum.feed(programResource->m_samplers[i].stages);
		}

		checksum.feed(programResource->m_textures.size());
		for (uint32_t i = 0; i < programResource->m_textures.size(); ++i)
		{
			checksum.feed(L"T");
			checksum.feed(i);
			checksum.feed(programResource->m_textures[i].binding);
			checksum.feed(programResource->m_textures[i].stages);
		}

		checksum.feed(programResource->m_images.size());
		for (uint32_t i = 0; i < programResource->m_images.size(); ++i)
		{
			checksum.feed(L"I");
			checksum.feed(i);
			checksum.feed(programResource->m_images[i].binding);
			checksum.feed(programResource->m_images[i].stages);
		}

		checksum.feed(programResource->m_sbuffers.size());
		for (uint32_t i = 0; i < programResource->m_sbuffers.size(); ++i)
		{
			checksum.feed(L"SB");
			checksum.feed(i);
			checksum.feed(programResource->m_sbuffers[i].binding);
			checksum.feed(programResource->m_sbuffers[i].stages);
		}

		checksum.feed(programResource->m_accelerationStructures.size());
		for (uint32_t i = 0; i < programResource->m_accelerationStructures.size(); ++i)
		{
			checksum.feed(L"AS");
			checksum.feed(i);
			checksum.feed(programResource->m_accelerationStructures[i].binding);
			checksum.feed(programResource->m_accelerationStructures[i].stages);
		}

		checksum.end();
		programResource->m_layoutHash = checksum.get();
	}

	// #note Need to delete program before shaders due to glslang weirdness.
	delete program;
	delete fragmentShader;
	delete vertexShader;
	delete computeShader;
	return programResource;
}

bool ProgramCompilerVk::generate(
	const ShaderGraph* shaderGraph,
	const ShaderModule* shaderModule,
	const PropertyGroup* settings,
	const std::wstring& name,
	Output& output) const
{
	RefArray< VertexOutput > vertexOutputs;
	RefArray< PixelOutput > pixelOutputs;
	RefArray< ComputeOutput > computeOutputs;
	RefArray< Script > scriptOutputs[5];

	for (auto node : shaderGraph->getNodes())
	{
		if (auto vertexOutput = dynamic_type_cast< VertexOutput* >(node))
			vertexOutputs.push_back(vertexOutput);
		else if (auto pixelOutput = dynamic_type_cast< PixelOutput* >(node))
			pixelOutputs.push_back(pixelOutput);
		else if (auto computeOutput = dynamic_type_cast< ComputeOutput* >(node))
			computeOutputs.push_back(computeOutput);
		else if (auto script = dynamic_type_cast< Script* >(node))
		{
			if (!script->getTechnique().empty())
			{
				if (script->getDomain() == Script::Undefined)
					return false;
				scriptOutputs[(int32_t)script->getDomain()].push_back(script);
			}
		}
	}

	GlslContext cx(shaderGraph, shaderModule, settings);

	if (vertexOutputs.size() == 1 && pixelOutputs.size() == 1)
	{
		bool result = true;
		result &= cx.getEmitter().emit(cx, pixelOutputs[0]);
		result &= cx.getEmitter().emit(cx, vertexOutputs[0]);
		if (!result)
		{
			log::error << L"Unable to generate Vulkan GLSL shader (" << name << L"); GLSL emitter failed." << Endl;
			return false;
		}
	}

	if (computeOutputs.size() >= 1 || scriptOutputs[Script::Compute].size() >= 1)
	{
		bool result = true;
		for (auto computeOutput : computeOutputs)
			result &= cx.getEmitter().emit(cx, computeOutput);
		for (auto scriptOutput : scriptOutputs[Script::Compute])
			result &= cx.getEmitter().emit(cx, scriptOutput);
		if (!result)
		{
			log::error << L"Unable to generate Vulkan GLSL shader (" << name << L"); GLSL emitter failed." << Endl;
			return false;
		}
	}

	const auto& layout = cx.getLayout();
	StringOutputStream ss;

#if 1
	ss << L"// Layout" << Endl;
	for (auto resource : layout.get())
	{
		if (const auto sampler = dynamic_type_cast< const GlslSampler* >(resource))
		{
			const SamplerState& state = sampler->getState();
			ss << L"// [binding = " << sampler->getBinding() << L", set = " << (int32_t)sampler->getSet() << L"] = sampler" << Endl;
			ss << L"//   .name = \"" << sampler->getName() << L"\"" << Endl;
			ss << L"//   .minFilter = " << lookupFilterName(state.minFilter) << Endl;
			ss << L"//   .mipFilter = " << lookupFilterName(state.mipFilter) << Endl;
			ss << L"//   .magFilter = " << lookupFilterName(state.magFilter) << Endl;
			ss << L"//   .addressU = " << lookupAddressName(state.addressU) << Endl;
			ss << L"//   .addressV = " << lookupAddressName(state.addressV) << Endl;
			ss << L"//   .addressW = " << lookupAddressName(state.addressW) << Endl;
			ss << L"//   .compare = " << lookupCompareFunctionName(state.compare) << Endl;
			ss << L"//   .mipBias = " << state.mipBias << Endl;
			ss << L"//   .ignoreMips = " << (state.ignoreMips ? L"YES" : L"NO") << Endl;
			ss << L"//   .useAnisotropic = " << (state.ignoreMips ? L"YES" : L"NO") << Endl;
		}
		else if (const auto texture = dynamic_type_cast< const GlslTexture* >(resource))
		{
			ss << L"// [binding = " << texture->getBinding() << L", set = " << (int32_t)texture->getSet() << L"] = texture" << Endl;
			ss << L"//   .name = \"" << texture->getName() << L"\"" << Endl;
			ss << L"//   .type = " << int32_t(texture->getUniformType()) << Endl;
		}
		else if (const auto uniformBuffer = dynamic_type_cast< const GlslUniformBuffer* >(resource))
		{
			ss << L"// [binding = " << uniformBuffer->getBinding() << L", set = " << (int32_t)uniformBuffer->getSet() << L"] = uniform buffer" << Endl;
			ss << L"//   .name = \"" << uniformBuffer->getName() << L"\"" << Endl;
			ss << L"//   .uniforms = {" << Endl;
			for (auto uniform : uniformBuffer->get())
				ss << L"//      " << int32_t(uniform.type) << L" \"" << uniform.name << L"\" " << uniform.length << Endl;
			ss << L"//   }" << Endl;
		}
		else if (const auto image = dynamic_type_cast< const GlslImage* >(resource))
		{
			ss << L"// [binding = " << image->getBinding() << L", set = " << (int32_t)image->getSet() << L"] = image" << Endl;
			ss << L"//   .name = \"" << image->getName() << L"\"" << Endl;
			ss << L"//   .type = " << int32_t(image->getUniformType()) << Endl;
		}
		else if (const auto storageBuffer = dynamic_type_cast< const GlslStorageBuffer* >(resource))
		{
			ss << L"// [binding = " << storageBuffer->getBinding() << L", set = " << (int32_t)storageBuffer->getSet() << L"] = storage buffer" << Endl;
			ss << L"//   .name = \"" << storageBuffer->getName() << L"\"" << Endl;
		}
		else if (const auto accelerationStructure = dynamic_type_cast< const GlslAccelerationStructure* >(resource))
		{
			ss << L"// [binding = " << accelerationStructure->getBinding() << L", set = " << (int32_t)accelerationStructure->getSet() << L"] = acceleration structure" << Endl;
			ss << L"//   .name = \"" << accelerationStructure->getName() << L"\"" << Endl;
		}
	}

	ss << Endl;
	ss << L"// Parameters" << Endl;
	for (auto p : cx.getParameters())
	{
		const wchar_t* c_parameterTypeNames[] = { L"scalar", L"vector", L"matrix", L"texture2d", L"texture3d", L"textureCube", L"sbuffer", L"image2d", L"image3d", L"imageCube", L"accelerationStructureEXT" };
		ss << L"// " << c_parameterTypeNames[(int32_t)p.type] << L" " << p.name << L", length = " << p.length << L", frequency = " << (int32_t)p.frequency << Endl;
	}
#endif

	GlslRequirements requirements = cx.requirements();

	// Vertex
	if (vertexOutputs.size() == 1 && pixelOutputs.size() == 1)
	{
		StringOutputStream vss;
		vss << cx.getVertexShader().getGeneratedShader(settings, layout, requirements);
		vss << Endl;
		vss << ss.str();
		vss << Endl;
		output.vertex = vss.str();
	}

	// Pixel
	if (vertexOutputs.size() == 1 && pixelOutputs.size() == 1)
	{
		StringOutputStream fss;
		fss << cx.getFragmentShader().getGeneratedShader(settings, layout, requirements);
		fss << Endl;
		fss << ss.str();
		fss << Endl;
		output.pixel = fss.str();
	}

	// Compute
	if (computeOutputs.size() >= 1 || scriptOutputs[Script::Compute].size() >= 1)
	{
		StringOutputStream css;
		css << cx.getComputeShader().getGeneratedShader(settings, layout, requirements);
		css << Endl;
		css << ss.str();
		css << Endl;
		output.compute = css.str();
	}

	return true;
}

}
