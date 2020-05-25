#include "Core/Log/Log.h"
#include "Core/Math/TransformPath.h"
#include "Editor/IPipelineBuilder.h"
#include "Model/Model.h"
#include "Model/Operations/MergeModel.h"
#include "Shape/Editor/Spline/ControlPointComponentData.h"
#include "Shape/Editor/Spline/SplineEntityData.h"
#include "Shape/Editor/Spline/SplineEntityReplicator.h"
#include "Shape/Editor/Spline/SplineLayerComponentData.h"
#include "World/Entity/GroupComponentData.h"

namespace traktor
{
    namespace shape
    {

T_IMPLEMENT_RTTI_FACTORY_CLASS(L"traktor.shape.SplineEntityReplicator", 0, SplineEntityReplicator, scene::IEntityReplicator)

TypeInfoSet SplineEntityReplicator::getSupportedTypes() const
{
    return makeTypeInfoSet< SplineEntityData >();
}

Ref< model::Model > SplineEntityReplicator::createModel(
    editor::IPipelineBuilder* pipelineBuilder,
    const std::wstring& assetPath,
    const Object* source
) const
{
	auto splineEntityData = mandatory_non_null_type_cast< const SplineEntityData* >(source);
	TransformPath path;

	// Count number of control points as we need to estimate fraction of each.
	int32_t controlPointCount = 0;
	for (auto entityData : splineEntityData->getEntityData())
	{
		for (auto componentData : entityData->getComponents())
		{
			if (is_a< ControlPointComponentData >(componentData))
				controlPointCount++;
		}
	}
	if (controlPointCount <= 0)
	{
		log::error << L"Invalid spline; no control points found." << Endl;
		return nullptr;	
	}

	// Create transformation path.
	int32_t controlPointIndex = 0;
	for (auto entityData : splineEntityData->getEntityData())
	{
		auto controlPointData = entityData->getComponent< ControlPointComponentData >();
		if (!controlPointData)
			continue;

		Transform T = entityData->getTransform();

		TransformPath::Key k;
		k.T = (float)controlPointIndex / (controlPointCount - 1);
		k.position = T.translation();
		k.orientation = T.rotation().toEulerAngles();
		k.values[0] = controlPointData->getScale();
		path.insert(k);

		++controlPointIndex;
	}

	// Create model, add geometry for each layer.
    Ref< model::Model > outputModel = new model::Model();

	for (auto componentData : splineEntityData->getComponents())
	{
		auto layerData = dynamic_type_cast< SplineLayerComponentData* >(componentData);
		if (!layerData)
			continue;

		Ref< model::Model > layerModel = layerData->createModel(pipelineBuilder->getSourceDatabase(), assetPath, path);
		if (!layerModel)
			continue;

		model::MergeModel merge(*layerModel, splineEntityData->getTransform().inverse(), 0.01f);
		merge.apply(*outputModel);
	}

    return outputModel;
}

Ref< Object > SplineEntityReplicator::modifyOutput(
    editor::IPipelineBuilder* pipelineBuilder,
    const std::wstring& assetPath,
    const Object* source,
    const model::Model* model
) const
{
    return nullptr;
}

    }
}