#pragma once

#include "Core/Containers/AlignedVector.h"
#include "Render/Editor/Glsl/GlslResource.h"
#include "Render/Editor/Glsl/GlslType.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_RENDER_EDITOR_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor
{
	namespace render
	{
	
class T_DLLCLASS GlslUniformBuffer : public GlslResource
{
	T_RTTI_CLASS;

public:
	struct Uniform
	{
		std::wstring name;
		GlslType type;
		int32_t length;
	};

	explicit GlslUniformBuffer(const std::wstring& name, uint8_t stages, int32_t ordinal);

	bool add(const std::wstring& uniformName, GlslType uniformType, int32_t length);

	const AlignedVector< Uniform >& get() const { return m_uniforms; }

	virtual int32_t getOrdinal() const override final { return m_ordinal; }

private:
	AlignedVector< Uniform > m_uniforms;
	int32_t m_ordinal;
};

	}
}