#pragma once

#include "Core/Ref.h"
#include "Render/Vulkan/StructBufferVk.h"
#include "Render/Vulkan/Private/Buffer.h"

namespace traktor
{
	namespace render
	{

class Buffer;

class StructBufferStaticVk : public StructBufferVk
{
	T_RTTI_CLASS;

public:
	explicit StructBufferStaticVk(Context* context, uint32_t bufferSize, uint32_t& instances);

	virtual ~StructBufferStaticVk();

	bool create();

	virtual void destroy() override final;

	virtual void* lock() override final;

	virtual void unlock() override final;

	virtual VkBuffer getVkBuffer() const override final { return *m_deviceBuffer; }

	virtual uint32_t getVkBufferOffset() const override final { return 0; }

private:
	Ref< Buffer > m_stageBuffer;
	Ref< Buffer > m_deviceBuffer;
};

	}
}