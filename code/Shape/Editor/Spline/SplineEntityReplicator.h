#pragma once

#include "Scene/Editor/IEntityReplicator.h"

// import/export mechanism.
#undef T_DLLCLASS
#if defined(T_SHAPE_EDITOR_EXPORT)
#	define T_DLLCLASS T_DLLEXPORT
#else
#	define T_DLLCLASS T_DLLIMPORT
#endif

namespace traktor
{
    namespace shape
    {

class T_DLLCLASS SplineEntityReplicator : public scene::IEntityReplicator
{
    T_RTTI_CLASS;

public:
    virtual TypeInfoSet getSupportedTypes() const override final;

    virtual Ref< model::Model > createModel(
        editor::IPipelineBuilder* pipelineBuilder,
        const std::wstring& assetPath,
        const Object* source
    ) const override final;

    virtual Ref< Object > modifyOutput(
        editor::IPipelineBuilder* pipelineBuilder,
        const std::wstring& assetPath,
        const Object* source,
        const model::Model* model
    ) const override final;
};

    }
}