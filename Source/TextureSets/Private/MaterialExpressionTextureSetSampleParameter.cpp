// (c) Electronic Arts. All Rights Reserved.

#include "MaterialExpressionTextureSetSampleParameter.h"

#include "HLSLTree/HLSLTreeCommon.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "TextureSet.h"
#include "TextureSetDefinition.h"
#include "TextureSetMaterialGraphBuilder.h"
#include "TextureSetModule.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TextureSets"

UMaterialExpressionTextureSetSampleParameter::UMaterialExpressionTextureSetSampleParameter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ParameterName = "Texture Set";

	bShowOutputNameOnPin = true;
	bShowOutputs         = true;

	bIsParameterExpression = true;

	// Update parameter on construction if it's invalid
	UpdateParameterGuid(false, true);

#if WITH_EDITOR
	OnTextureSetDefinitionChangedHandle = UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.AddUObject(this, &UMaterialExpressionTextureSetSampleParameter::OnDefinitionChanged);
#endif
}

FName UMaterialExpressionTextureSetSampleParameter::GetTextureParameterName(int TextureIndex) const
{
	return MakeTextureParameterName(ParameterName, TextureIndex);
}

FName UMaterialExpressionTextureSetSampleParameter::GetConstantParameterName(FName ConstantName) const
{
	return MakeConstantParameterName(ParameterName, ConstantName);
}

FName UMaterialExpressionTextureSetSampleParameter::MakeTextureParameterName(FName ParameterName, int TextureIndex)
{
	return FName(FString::Format(TEXT("TEXSET_{0}_PACKED_{1}"), {ParameterName.ToString(), FString::FromInt(TextureIndex)}));
}

FName UMaterialExpressionTextureSetSampleParameter::MakeConstantParameterName(FName ParameterName, FName ConstantName)
{
	return FName(FString::Format(TEXT("TEXSET_{0}_{1}"), {ParameterName.ToString(), ConstantName.ToString()}));
}

bool UMaterialExpressionTextureSetSampleParameter::IsTextureSetParameterName(FName Name)
{
	return Name.ToString().StartsWith("TEXSET_", ESearchCase::IgnoreCase);
}

#if WITH_EDITOR
uint32 UMaterialExpressionTextureSetSampleParameter::ComputeMaterialFunctionHash()
{
	if (!IsValid(Definition))
		return 0;

	uint32 Hash = GetTypeHash(FString("TextureSetSampleParameter_V0.2"));
	Hash = HashCombine(Hash, GetTypeHash(Definition->GetUserKey()));

	Hash = HashCombine(Hash, GetTypeHash(FTextureSetMaterialGraphBuilder::GetGraphBuilderVersion()));

	Hash = HashCombine(Hash, GetTypeHash(ParameterName.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(Group.ToString()));
	Hash = HashCombine(Hash, GetTypeHash(SortPriority));

	FTextureSetPackingInfo PackingInfo = Definition->GetPackingInfo();
	for (int i = 0; i < PackingInfo.NumPackedTextures(); i++)
	{
		Hash = HashCombine(Hash, GetTypeHash(PackingInfo.GetPackedTextureDef(i)));
	}

	for (const UTextureSetModule* Module : Definition->GetModuleInfo().GetModules())
	{
		Hash = HashCombine(Hash, Module->ComputeSamplingHash(this));
	}

	Hash = HashCombine(Hash, GetTypeHash(BaseNormalSource));
	Hash = HashCombine(Hash, GetTypeHash(TangentSource));
	Hash = HashCombine(Hash, GetTypeHash(PositionSource));
	Hash = HashCombine(Hash, GetTypeHash(CameraVectorSource));

	return Hash;
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::ConfigureMaterialFunction(class UMaterialFunction* NewMaterialFunction)
{
	FDataValidationContext ValidationContext;
	if (IsValid(Definition) && Definition->IsDataValid(ValidationContext) == EDataValidationResult::Invalid)
	{
		return;
	}
	// Make sure our samping parameters are up to date before generating the sampling graph
	UpdateSampleParamArray();

	FTextureSetMaterialGraphBuilder(NewMaterialFunction, this);
}
#endif

FGuid& UMaterialExpressionTextureSetSampleParameter::GetParameterExpressionId()
{
	return ExpressionGUID;
}

#if WITH_EDITOR
FName UMaterialExpressionTextureSetSampleParameter::GetParameterName() const
{
	return ParameterName;
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::SetParameterName(const FName& Name)
{
	ParameterName = Name;
}
#endif

#if WITH_EDITOR
bool UMaterialExpressionTextureSetSampleParameter::GetParameterValue(FMaterialParameterMetadata& OutMeta) const
{
	OutMeta.Value = DefaultTextureSet.Get();
	OutMeta.Description = Desc;
	OutMeta.ExpressionGuid = ExpressionGUID;
	OutMeta.Group = Group;
	OutMeta.SortPriority = SortPriority;
	OutMeta.AssetPath = GetAssetPathName();
	// Parameter class will be null when executable is compiled with editor, but run standalone, as the TextureSetsEditor module will not be loaded.
	OutMeta.CustomParameterClass = FindObject<UClass>(FTopLevelAssetPath("/Script/TextureSetsEditor.DEditorTextureSetParameterValue"));
	return true;
}
#endif

#if WITH_EDITOR
bool UMaterialExpressionTextureSetSampleParameter::SetParameterValue(const FName& Name, const FMaterialParameterMetadata& Meta, EMaterialExpressionSetParameterValueFlags Flags)
{
	if (Meta.Value.Type == EMaterialParameterType::Custom)
	{
		// Null values are valid, objects that fail to cast to a texture set are not
		UTextureSet* TextureSetValue = Cast<UTextureSet>(Meta.Value.AsCustomObject());
		if (IsValid(TextureSetValue) || Meta.Value.AsCustomObject() == nullptr)
		{
			if (Name == ParameterName)
			{
				DefaultTextureSet = TextureSetValue;

				if (EnumHasAnyFlags(Flags, EMaterialExpressionSetParameterValueFlags::SendPostEditChangeProperty))
				{
					FProperty* Property = FindFProperty<FProperty>(StaticClass(), TEXT("DefaultTextureSet"));
					FPropertyChangedEvent Event(Property);
					PostEditChangeProperty(Event);
				}

				if (EnumHasAnyFlags(Flags, EMaterialExpressionSetParameterValueFlags::AssignGroupAndSortPriority))
				{
					Group = Meta.Group;
					SortPriority = Meta.SortPriority;
				}

				return true;
			}
		}
	}
	return false;
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(ParameterName.ToString());
}
#endif

void UMaterialExpressionTextureSetSampleParameter::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	UTextureSetDefinition::FOnTextureSetDefinitionChangedEvent.Remove(OnTextureSetDefinitionChangedHandle);
#endif
}

#if WITH_EDITOR
EDataValidationResult UMaterialExpressionTextureSetSampleParameter::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (ParameterName.IsNone())
	{
		Context.AddError(LOCTEXT("BadParamName","Texture Set Parameter name cannot be 'None', please specify a valid name."));
		Result = EDataValidationResult::Invalid;
	}

	if (!IsValid(Definition))
	{
		Context.AddError(LOCTEXT("MissingDefinition","A texture set sample must reference a valid definition."));
		Result = EDataValidationResult::Invalid;
	}
	else if (Definition->IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Invalid;
	}

	if (DefaultTextureSet.IsValid() && DefaultTextureSet->Definition != Definition)
	{
		Context.AddError(LOCTEXT("MismatchedDefinition","The Default texture set does not use the same definition as specified in the material expression parameter."));
		Result = EDataValidationResult::Invalid;
	}

	return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::UpdateSampleParamArray()
{
	if (!IsValid(Definition))
		return;

	TArray<TSubclassOf<UTextureSetAssetParams>> RequiredSampleParamClasses(Definition->GetRequiredSampleParamClasses());

	SampleParams.UpdateParamList(this, RequiredSampleParamClasses);
}
#endif

#if WITH_EDITOR
void UMaterialExpressionTextureSetSampleParameter::OnDefinitionChanged(UTextureSetDefinition* ChangedDefinition)
{
	// Only update if this is our definition, or if we're not going to call post-load later (that will update everything anyway).
	if (ChangedDefinition == Definition && !HasAnyFlags(RF_NeedPostLoad))
	{
		if (UpdateMaterialFunction())
		{
			UMaterialEditingLibrary::RecompileMaterial(Material);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
