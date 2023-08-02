// (c) Electronic Arts. All Rights Reserved.

#include "TextureSetsMaterialInstanceUserData.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/ObjectSaveContext.h"
#include "MaterialExpressionTextureSetSampleParameter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialFunctionInterface.h"
#include "TextureSetDefinition.h"
#include "TextureSet.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#endif

#if WITH_EDITOR
const TMap<FMaterialParameterInfo, const UMaterialExpressionTextureSetSampleParameter*> UTextureSetsMaterialInstanceUserData::FindAllSampleExpressions(UMaterialInstance* MaterialInstance)
{
	TMap<FMaterialParameterInfo, const UMaterialExpressionTextureSetSampleParameter*> Result;

	UMaterial* Material = MaterialInstance->GetMaterial();

	if (IsValid(Material))
	{
		// Find all expressions in material
		{
			TArray<const UMaterialExpressionTextureSetSampleParameter*> SamplerExpressions;
			Material->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(SamplerExpressions);
		
			for (const UMaterialExpressionTextureSetSampleParameter* Expression : SamplerExpressions)
			{
				FMaterialParameterInfo Info;
				Info.Name = Expression->ParameterName;
				Info.Association = EMaterialParameterAssociation::GlobalParameter;
				Info.Index = INDEX_NONE;
				Result.Add(Info, Expression);
			}
		}

		FMaterialLayersFunctions Layers;
		MaterialInstance->GetMaterialLayers(Layers);
		
		// Find all expressions in layers
		for (int i = 0; i < Layers.Layers.Num(); i++)
		{
			if (!Layers.Layers[i])
				continue;

			UMaterialFunction* LayerFunction = Layers.Layers[i]->GetBaseFunction();
		
			TArray<UMaterialExpressionTextureSetSampleParameter*> SamplerExpressions;
			LayerFunction->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(SamplerExpressions);
		
			for (const UMaterialExpressionTextureSetSampleParameter* Expression : SamplerExpressions)
			{
				FMaterialParameterInfo Info;
				Info.Name = Expression->ParameterName;
				Info.Association = EMaterialParameterAssociation::LayerParameter;
				Info.Index = i;
				Result.Add(Info, Expression);
			}
		}

		// Find all expression in layer blends
		for (int i = 0; i < Layers.Blends.Num(); i++)
		{
			if (!Layers.Blends[i])
				continue;

			UMaterialFunction* BlendFunction = Layers.Blends[i]->GetBaseFunction();

			TArray<UMaterialExpressionTextureSetSampleParameter*> SamplerExpressions;
			BlendFunction->GetAllExpressionsOfType<UMaterialExpressionTextureSetSampleParameter>(SamplerExpressions);

			for (const UMaterialExpressionTextureSetSampleParameter* Expression : SamplerExpressions)
			{
				FMaterialParameterInfo Info;
				Info.Name = Expression->ParameterName;
				Info.Association = EMaterialParameterAssociation::BlendParameter;
				Info.Index = i;
				Result.Add(Info, Expression);
			}
		}
	}

	return Result;
}
#endif

#if WITH_EDITOR
void UTextureSetsMaterialInstanceUserData::UpdateAssetUserData(UMaterialInstance* MaterialInstance)
{
	check(MaterialInstance);

	if (!IsValid(MaterialInstance->GetMaterial()))
		return; // Possible if parent has not been assigned yet.

	const TMap<FMaterialParameterInfo, const UMaterialExpressionTextureSetSampleParameter*> TextureSetExpressions = UTextureSetsMaterialInstanceUserData::FindAllSampleExpressions(MaterialInstance);

	UTextureSetsMaterialInstanceUserData* TextureSetUserData = MaterialInstance->GetAssetUserData<UTextureSetsMaterialInstanceUserData>();

	// If there are no texture set expressions, we can clear TextureSetUserData
	if (TextureSetExpressions.IsEmpty() && IsValid(TextureSetUserData))
	{
		MaterialInstance->RemoveUserDataOfClass(UTextureSetsMaterialInstanceUserData::StaticClass());
		return; // Done here, not a material that needs texture set stuff.
	}

	TArray<FMaterialParameterInfo> UnusedOverrides;

	if (IsValid(TextureSetUserData))
	{
		UnusedOverrides = TextureSetUserData->GetOverrides();
	}

	for (const auto& [NodeParameterInfo, TextureSetExpression] : TextureSetExpressions)
	{
		// Add texture set user data if it doesn't exist
		// Note that this happens inside the loop of texture set expressions.
		// This is so the user data is only added if there is a texture set expression.
		if (!IsValid(TextureSetUserData))
		{
			TextureSetUserData = NewObject<UTextureSetsMaterialInstanceUserData>(MaterialInstance);
			MaterialInstance->AddAssetUserData(TextureSetUserData);
		}

		if (TextureSetUserData->HasOverride(NodeParameterInfo))
		{
			UnusedOverrides.RemoveSingle(NodeParameterInfo);
		}
		else
		{
			// Add new override
			FTextureSetOverride Override;

			// TODO: See if there are any old overrides which have been re-named, which we can recover values from based on node GUIDs
			Override.Info = NodeParameterInfo;
			Override.TextureSet = nullptr;
			Override.IsOverridden = false;

			TextureSetUserData->SetOverride(Override);
		}
	}

	// Clear any overrides we no longer need.
	for (const FMaterialParameterInfo& Unused : UnusedOverrides)
	{
		TextureSetUserData->RemoveOverride(Unused);
	}
}
#endif

void UTextureSetsMaterialInstanceUserData::PostInitProperties()
{
	Super::PostInitProperties();
	if (GetClass()->GetDefaultObject() != this)
	{
		MaterialInstance = Cast<UMaterialInstance>(GetOuter());
		checkf(MaterialInstance, TEXT("Texture set user data must be created with a UMaterialInstance as the outer."))
	}
}

void UTextureSetsMaterialInstanceUserData::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	// Clear any texture set parameter from the material instance
	// We want to make sure they're only set at runtime and not serialized.
	// TODO: We need a post-save to restore this somehow. Otherwise, they are missing until post-load is called.
	// We actually just want to ignore certain ones on serialize, so there may be a better approach.
	// When this is figure out, we can remove the "public" flags on the derived textures generated by the texture set.
	//ClearTextureSetParameters();
}

void UTextureSetsMaterialInstanceUserData::PostLoadOwner()
{
	Super::PostLoadOwner();
#if WITH_EDITOR
	// Update the texture set parameters when the MaterialInstance has finished loading.
	UpdateTextureSetParameters();
#endif
}

void UTextureSetsMaterialInstanceUserData::ClearTextureSetParameters()
{
	// Clears all parameters in a material instance that are driven by a texture set.

	for(int i = 0; i < MaterialInstance->ScalarParameterValues.Num(); i++)
	{
		const FScalarParameterValue& TextureParameter = MaterialInstance->ScalarParameterValues[i];
		if (UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(TextureParameter.ParameterInfo.Name))
		{
			MaterialInstance->ScalarParameterValues.RemoveAt(i);
			i--;
		}
	}

	for(int i = 0; i < MaterialInstance->VectorParameterValues.Num(); i++)
	{
		const FVectorParameterValue& TextureParameter = MaterialInstance->VectorParameterValues[i];
		if (UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(TextureParameter.ParameterInfo.Name))
		{
			MaterialInstance->VectorParameterValues.RemoveAt(i);
			i--;
		}
	}

	for(int i = 0; i < MaterialInstance->DoubleVectorParameterValues.Num(); i++)
	{
		const FDoubleVectorParameterValue& TextureParameter = MaterialInstance->DoubleVectorParameterValues[i];
		if (UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(TextureParameter.ParameterInfo.Name))
		{
			MaterialInstance->DoubleVectorParameterValues.RemoveAt(i);
			i--;
		}
	}

	for(int i = 0; i < MaterialInstance->TextureParameterValues.Num(); i++)
	{
		const FTextureParameterValue& TextureParameter = MaterialInstance->TextureParameterValues[i];
		if (UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(TextureParameter.ParameterInfo.Name))
		{
			MaterialInstance->TextureParameterValues.RemoveAt(i);
			i--;
		}
	}

	for(int i = 0; i < MaterialInstance->RuntimeVirtualTextureParameterValues.Num(); i++)
	{
		const FRuntimeVirtualTextureParameterValue& TextureParameter = MaterialInstance->RuntimeVirtualTextureParameterValues[i];
		if (UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(TextureParameter.ParameterInfo.Name))
		{
			MaterialInstance->RuntimeVirtualTextureParameterValues.RemoveAt(i);
			i--;
		}
	}
}

#if WITH_EDITOR
void UTextureSetsMaterialInstanceUserData::UpdateTextureSetParameters()
{
	if (!IsValid(MaterialInstance->GetMaterial()))
		return;
	
	// Remove any exsting texture set related parameters, as we're about to re-create the ones that are needed.
	ClearTextureSetParameters();

	const TMap<FMaterialParameterInfo, const UMaterialExpressionTextureSetSampleParameter*> Expressions = UTextureSetsMaterialInstanceUserData::FindAllSampleExpressions(MaterialInstance);

	for (const FTextureSetOverride& TextureSetOverride : TexturesSetOverrides)
	{
		if (!TextureSetOverride.IsOverridden || TextureSetOverride.TextureSet == nullptr)
			continue;

		if (!Expressions.Contains(TextureSetOverride.Info))
			continue;

		const UMaterialExpressionTextureSetSampleParameter* SampleExpression = Expressions.FindChecked(TextureSetOverride.Info);

		UTextureSetDefinition* Definition = SampleExpression->Definition;
		if (Definition == nullptr)
			continue;

		const UTextureSetDerivedData* DerivedData = TextureSetOverride.TextureSet->GetDerivedData();

		// Set any constant parameters what we have
		for (auto& [ParameterName, Value] : DerivedData->MaterialParameters)
		{
			FVectorParameterValue Parameter;
			Parameter.ParameterValue = FLinearColor(Value);
			Parameter.ParameterInfo.Name = SampleExpression->GetConstantParameterName(TextureSetOverride.Info.Name);
			Parameter.ParameterInfo.Association = TextureSetOverride.Info.Association;
			Parameter.ParameterInfo.Index = TextureSetOverride.Info.Index;
			MaterialInstance->VectorParameterValues.Add(Parameter);
		}

		for (int i = 0; i < DerivedData->PackedTextureData.Num(); i++)
		{
			const FPackedTextureData& PackedTextureData = DerivedData->PackedTextureData[i];

			// Set the texture parameter for each packed texture
			FTextureParameterValue TextureParameter;
			TextureParameter.ParameterValue = TextureSetOverride.IsOverridden ? TextureSetOverride.TextureSet->GetDerivedTexture(i) : nullptr;
			TextureParameter.ParameterInfo.Name = SampleExpression->GetTextureParameterName(i);
			TextureParameter.ParameterInfo.Association = TextureSetOverride.Info.Association;
			TextureParameter.ParameterInfo.Index = TextureSetOverride.Info.Index;
			MaterialInstance->TextureParameterValues.Add(TextureParameter);

			// Set any constant parameters that come with this texture
			for (auto& [ParameterName, Value] : PackedTextureData.MaterialParameters)
			{
				FVectorParameterValue Parameter;
				Parameter.ParameterValue = FLinearColor(Value);
				Parameter.ParameterInfo.Name = SampleExpression->GetConstantParameterName(ParameterName);
				Parameter.ParameterInfo.Association = TextureSetOverride.Info.Association;
				Parameter.ParameterInfo.Index = TextureSetOverride.Info.Index;
				MaterialInstance->VectorParameterValues.Add(Parameter);
			}
		}
	}
}
#endif

const TArray<FMaterialParameterInfo> UTextureSetsMaterialInstanceUserData::GetOverrides() const
{
	TArray<FMaterialParameterInfo> Infos;

	for (FTextureSetOverride Override : TexturesSetOverrides)
		Infos.Add(Override.Info);

	return Infos;
}

bool UTextureSetsMaterialInstanceUserData::HasOverride(const FMaterialParameterInfo& Info) const
{
	FTextureSetOverride Dummy;
	return GetOverride(Info, Dummy);
}

bool UTextureSetsMaterialInstanceUserData::GetOverride(const FMaterialParameterInfo& Info, FTextureSetOverride& OutOverride) const
{
	for (FTextureSetOverride Override : TexturesSetOverrides)
	{
		if (Override.Info == Info)
		{
			OutOverride = Override;
			return true;
		}
	}

	return false;
}

void UTextureSetsMaterialInstanceUserData::SetOverride(const FTextureSetOverride& Override)
{
	bool FoundExisting = false;

	for (int i = 0; i < TexturesSetOverrides.Num(); i++)
	{
		if (TexturesSetOverrides[i].Info == Override.Info)
		{
			TexturesSetOverrides[i] = Override;
			FoundExisting = true;
			break;
		}
	}

	if (!FoundExisting)
	{
		TexturesSetOverrides.Add(Override);
	}

#if WITH_EDITOR
	UpdateTextureSetParameters();

	FPropertyChangedEvent DummyEvent(nullptr);
	MaterialInstance->PostEditChangeProperty(DummyEvent);

	// Trigger a redraw of the UI
	UMaterialEditingLibrary::RefreshMaterialEditor(MaterialInstance);
#endif
}

void UTextureSetsMaterialInstanceUserData::RemoveOverride(const FMaterialParameterInfo& Info)
{
	for (int i = 0; i < TexturesSetOverrides.Num(); i++)
	{
		if (TexturesSetOverrides[i].Info == Info)
		{
			TexturesSetOverrides.RemoveAt(i);
		}
	}
}
