// (c) Electronic Arts. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TextureSetInfo.h"
#include "TextureSetPackedTextureDef.h"

#include "TextureSetDefinition.generated.h"

class UTextureSet;
class UTextureSetModule;
class UTextureSetAssetParams;
class UTextureSetSampleParams;

#if WITH_EDITOR
DECLARE_EVENT_OneParam(UTextureSetDefinition, FOnTextureSetDefinitionChanged, UTextureSetDefinition* /*Definition*/);
#endif

// The texture set definition. Definitions consist primarily of a list of modules, and a packing definition.
// The definition is configurable, and drives most other aspects of the texture-set from import to packing to sampling.
UCLASS()
class TEXTURESETS_API UTextureSetDefinition : public UObject
{
	GENERATED_BODY()

	UTextureSetDefinition();

public:

#if WITH_EDITOR
	static FOnTextureSetDefinitionChanged FOnTextureSetDefinitionChangedEvent;
#endif

	// UObject Overrides
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif
	virtual void PostLoad() override;

#if WITH_EDITOR
	// Gets the list of channel names which have not yet been allocated a spot in the packing definitions.
	UFUNCTION(CallInEditor)
	TArray<FName> EditGetUnpackedChannelNames() const;
#endif

	const FORCEINLINE FTextureSetDefinitionModuleInfo& GetModuleInfo() const { return ModuleInfo; }
	const FORCEINLINE FTextureSetPackingInfo& GetPackingInfo() const { return PackingInfo; }

	TArray<TSubclassOf<UTextureSetAssetParams>> GetRequiredAssetParamClasses() const;
	TArray<TSubclassOf<UTextureSetSampleParams>> GetRequiredSampleParamClasses() const;

	// Get the default packed texture for a specific packed texture index
	const UTextureSet* GetDefaultTextureSet() const;

#if WITH_EDITOR
	void UpdateDefaultTextureSet();
	uint32 ComputeCookingHash();
#endif

	FGuid GetGuid() { return UniqueID; }

private:

	// Created once on construction, and used to compare definitions
	UPROPERTY(VisibleAnywhere)
	FGuid UniqueID;

#if WITH_EDITORONLY_DATA
	// Modules and packing definitions are duplicated here in editor, so they can be freely edited by the user without causing excessive rebuilds.
	// On load, ResetEdits() fills them in from the serialized data, and on pre-save, ApplyEdits() updates the serialized data.
	UPROPERTY(Transient, EditAnywhere, DisplayName="Modules", meta=(NoElementDuplicate))
	TArray<UTextureSetModule*> EditModules;

	UPROPERTY(Transient, EditAnywhere, DisplayName="Packing", meta=(TitleProperty="CompressionSettings", NoElementDuplicate))
	TArray<FTextureSetPackedTextureDef> EditPackedTextures;
#endif

	// For debugging, allow the user to manually change a value that doesn't affect the logic,
	// but is hashed. Forces a regeneration of the data when a new unique value is entered.
	UPROPERTY(EditAnywhere, Category="Debug")
	FString UserKey;

	UPROPERTY(VisibleAnywhere, Category="Debug")
	TObjectPtr<UTextureSet> DefaultTextureSet;

	UPROPERTY(VisibleAnywhere, Category="Debug")
	FTextureSetDefinitionModuleInfo ModuleInfo;

	UPROPERTY()
	FTextureSetPackingInfo PackingInfo;
	
	UPROPERTY(VisibleAnywhere, Category="Debug")
	uint32 CookingHash;

#if WITH_EDITOR
	void ApplyEdits();
	void ResetEdits();

	FTextureSetPackingInfo CreatePackingInfo(const TArray<FTextureSetPackedTextureDef>& PackedTextures, const FTextureSetProcessingGraph& ProcessingGraph);
#endif

};