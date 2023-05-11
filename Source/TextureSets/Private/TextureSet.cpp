//
// (c) Electronic Arts.  All Rights Reserved.
//

#include "TextureSet.h"

#include "MaterialPropertyHelpers.h"
#include "ReferencedAssetsUtils.h"
#include "TextureSetDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "MaterialEditorModule.h"
#include "TextureSetEditingUtils.h"

TObjectPtr<UTexture2D> LoadDefaultTexture()
{
	return LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), nullptr, LOAD_None, nullptr);
}

UTextureSet::UTextureSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

void UTextureSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UE_LOG(LogTemp, Warning, TEXT("Changed me: %s"), *PropertyChangedEvent.GetPropertyName().ToString());

	if (PropertyChangedEvent.GetPropertyName().IsEqual("Definition")
		&& PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		Textures.Empty();

		if (Definition)
		{
			for (auto& TextureInfo : Definition->Items)
			{
				FTextureData TextureData;
				TextureData.TextureAsset = LoadDefaultTexture();
				Textures.Add(TextureData);
			}
		}
	}
}

void UTextureSet::UpdateFromDefinition()
{
	if (Definition)
	{
		TArray<FTextureData> NewTextures;
		NewTextures.Reserve(Definition->Items.Num());
		for (auto& TextureInfo : Definition->Items)
		{
			if (FTextureData* ExistingTexture = Textures.FindByPredicate(
				[TextureInfo](FTextureData& TextureData)
				{
					return TextureData.TextureName.Equals(TextureInfo.TextureTypes);
				}))
			{
				NewTextures.Add(*ExistingTexture);
				continue;
			}
			FTextureData TextureData;
			TextureData.TextureAsset = LoadDefaultTexture();
			TextureData.TextureName.Append(TextureInfo.TextureTypes);
			NewTextures.Add(TextureData);
		}

		Textures.Empty();
		Textures.Append(NewTextures);

		//UpdateReferencingMaterials();
	}
}

void UTextureSet::UpdateReferencingMaterials()
{
	//FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> HardDependencies = FTextureSetEditingUtils::FindReferencers(GetPackage()->GetFName());//AssetRegistryModule.Get().GetReferencers(GetPackage()->GetFName(), HardDependencies);

	{
		IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");

		for (auto dependency : HardDependencies)
		{
			UObject* Referencer = UEditorAssetLibrary::LoadAsset(dependency.ToString());
			if (!Referencer->IsA(UMaterial::StaticClass()))
			{
				continue;
			}

			UMaterial* ReferencingMaterial = Cast<UMaterial>(Referencer);

			for (UMaterialExpression* Expression : ReferencingMaterial->GetExpressions())
			{
				ensure(Expression);
				Expression->PostEditChange();
			}

			ReferencingMaterial->Modify();
			//ReferencingMaterial->ForceRecompileForRendering();

			UEditorAssetLibrary::SaveLoadedAsset(Referencer);
		}
	}
}
