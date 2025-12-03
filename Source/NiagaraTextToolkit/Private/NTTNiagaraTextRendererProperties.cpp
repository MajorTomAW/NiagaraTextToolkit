// Copyright Epic Games, Inc. All Rights Reserved.

#include "NTTNiagaraTextRendererProperties.h"
#include "NTTNiagaraTextVertexFactory.h"
#include "NTTDataInterface.h"
#include "Materials/Material.h"
#include "NiagaraRenderer.h"
#include "NiagaraConstants.h"
#include "NiagaraGPUSortInfo.h"
#include "NTTNiagaraTextRenderer.h"
#include "NTTNiagaraBoundsCalculatorHelper.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"

#include "Engine/Texture2D.h"
#include "Internationalization/Internationalization.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Modules/ModuleManager.h"
#include "UObject/UE5MainStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NTTNiagaraTextRendererProperties)

#if WITH_EDITORONLY_DATA
#include "DerivedDataCacheInterface.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "NiagaraModule.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SWidget.h"
#include "AssetThumbnail.h"
#include "Widgets/Text/STextBlock.h"
#endif

#define LOCTEXT_NAMESPACE "UNTTNiagaraTextRendererProperties"

TArray<TWeakObjectPtr<UNTTNiagaraTextRendererProperties>> UNTTNiagaraTextRendererProperties::FontRendererPropertiesToDeferredInit;

UNTTNiagaraTextRendererProperties::UNTTNiagaraTextRendererProperties()
	: Material(nullptr)
	, MaterialUserParamBinding(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()))
	, bOverrideFontMaterialParameter(true)
	, OverrideFontParameterName(FName("NTT_Font"))
	, NTTDataInterfaceBinding(FNiagaraTypeDefinition(UNTTDataInterface::StaticClass()))
	, bRemoveHMDRollInVR(false)
	, bSortOnlyWhenTranslucent(true)
{
	AttributeBindings =
	{
		// NOTE: These bindings' indices have to align to their counterpart in ENTTNiagaraSpriteVFLayout
		&PositionBinding,
		&ColorBinding,
		&VelocityBinding,
		&SpriteRotationBinding,
		&SpriteSizeBinding,
		&SpriteFacingBinding,
		&SpriteAlignmentBinding,
		&DynamicMaterialBinding,
		&DynamicMaterial1Binding,
		&DynamicMaterial2Binding,
		&DynamicMaterial3Binding,
		&CameraOffsetBinding,
		&UVScaleBinding,
		&PivotOffsetBinding,
		&MaterialRandomBinding,
		&CustomSortingBinding,
		&NormalizedAgeBinding,
		&CharacterIndexBinding,

		// These bindings are only actually used with accurate motion vectors (indices still need to align)
		&PrevPositionBinding,
		&PrevVelocityBinding,
		&PrevSpriteRotationBinding,
		&PrevSpriteSizeBinding,
		&PrevSpriteFacingBinding,
		&PrevSpriteAlignmentBinding,
		&PrevCameraOffsetBinding,
		&PrevPivotOffsetBinding,
	};
}

FNiagaraRenderer* UNTTNiagaraTextRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer* NewRenderer = new FNTTNiagaraTextRenderer(FeatureLevel, this, Emitter);
	NewRenderer->Initialize(this, Emitter, InController);
	return NewRenderer;
}

FNiagaraBoundsCalculator* UNTTNiagaraTextRendererProperties::CreateBoundsCalculator()
{
	if (GetCurrentSourceMode() == ENiagaraRendererSourceDataMode::Emitter)
	{
		return nullptr;
	}

	return new FNTTNiagaraBoundsCalculatorHelper<false, false, false>();
}

void UNTTNiagaraTextRendererProperties::GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const
{
	UMaterialInterface* MaterialInterface = nullptr;
	if (InEmitter != nullptr)
	{
		MaterialInterface = Cast<UMaterialInterface>(InEmitter->FindBinding(MaterialUserParamBinding.Parameter));
	}

#if WITH_EDITORONLY_DATA
	MaterialInterface = MaterialInterface ? MaterialInterface : ToRawPtr(MICMaterial);
#endif

	OutMaterials.Add(MaterialInterface ? MaterialInterface : ToRawPtr(Material));
}

void UNTTNiagaraTextRendererProperties::CollectPSOPrecacheData(FPSOPrecacheParamsList& OutParams)
{
	const FVertexFactoryType* VFType = GetVertexFactoryType();
	UMaterialInterface* MaterialInterface = ToRawPtr(Material);

	if (MaterialInterface)
	{
		FPSOPrecacheParams& PSOPrecacheParams = OutParams.AddDefaulted_GetRef();
		PSOPrecacheParams.MaterialInterface = MaterialInterface;
		// Spite VF is the same for MVF and non-MVF cases
		PSOPrecacheParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(VFType));
	}
}

const FVertexFactoryType* UNTTNiagaraTextRendererProperties::GetVertexFactoryType() const
{
	return &FNTTNiagaraTextVertexFactory::StaticType;
}

void UNTTNiagaraTextRendererProperties::PostLoad()
{
	Super::PostLoad();

	if ( Material )
	{
		Material->ConditionalPostLoad();
	}

#if WITH_EDITORONLY_DATA
	if (MaterialUserParamBinding.Parameter.GetType().GetClass() != UMaterialInterface::StaticClass())
	{
		FNiagaraTypeDefinition MaterialDef(UMaterialInterface::StaticClass());
		MaterialUserParamBinding.Parameter.SetType(MaterialDef);
	}

	ChangeToPositionBinding(PositionBinding);
	ChangeToPositionBinding(PrevPositionBinding);
	PostLoadBindings(SourceMode);

	// Fix up these bindings from their loaded source bindings
	SetPreviousBindings(FVersionedNiagaraEmitter(), SourceMode);

	if (MaterialParameterBindings_DEPRECATED.Num() > 0)
	{
		MaterialParameters.AttributeBindings = MaterialParameterBindings_DEPRECATED;
		MaterialParameterBindings_DEPRECATED.Empty();
	}
#endif
}

void UNTTNiagaraTextRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		// We can end up hitting PostInitProperties before the Niagara Module has initialized bindings this needs, mark this object for deferred init and early out.
		if (FModuleManager::Get().IsModuleLoaded("Niagara") == false)
		{
			FontRendererPropertiesToDeferredInit.Add(this);
			return;
		}
		InitBindings();
	}
}

void UNTTNiagaraTextRendererProperties::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& Ar = Record.GetUnderlyingArchive();
	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	const int32 NiagaraVersion = Ar.CustomVer(FNiagaraCustomVersion::GUID);
	const int32 UE5MainVersion = Ar.CustomVer(FUE5MainStreamObjectVersion::GUID);

	if (Ar.IsLoading() && (NiagaraVersion < FNiagaraCustomVersion::DisableSortingByDefault))
	{
		SortMode = ENiagaraSortMode::ViewDistance;
	}

	if (Ar.IsLoading() && (UE5MainVersion < FUE5MainStreamObjectVersion::NiagaraSpriteRendererFacingAlignmentAutoDefault))
	{
		Alignment = ENTTNiagaraSpriteAlignment::Unaligned;
		FacingMode = ENTTNiagaraSpriteFacingMode::FaceCamera;
	}

	// MIC will replace the main material during serialize
	// Be careful if adding code that looks at the material to make sure you get the correct one
	{
	#if WITH_EDITORONLY_DATA
		TOptional<TGuardValue<TObjectPtr<UMaterialInterface>>> MICGuard;
		if (Ar.IsSaving() && Ar.IsCooking() && MICMaterial)
		{
			MICGuard.Emplace(Material, MICMaterial);
		}
	#endif

		Super::Serialize(Record);
	}

	bool bIsCookedForEditor = false;
#if WITH_EDITORONLY_DATA
	bIsCookedForEditor = ((Ar.GetPortFlags() & PPF_Duplicate) == 0) && GetPackage()->HasAnyPackageFlags(PKG_Cooked);
#endif // WITH_EDITORONLY_DATA

	if (Ar.IsCooking() || (FPlatformProperties::RequiresCookedData() && Ar.IsLoading()) || bIsCookedForEditor)
	{
		//DerivedData.Serialize(Record.EnterField(TEXT("DerivedData")));
	}
}

void UNTTNiagaraTextRendererProperties::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RendererLayoutWithCustomSort.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RendererLayoutWithoutCustomSort.GetAllocatedSize());
}

/** The bindings depend on variables that are created during the NiagaraModule startup. However, the CDO's are build prior to this being initialized, so we defer setting these values until later.*/
void UNTTNiagaraTextRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	UNTTNiagaraTextRendererProperties* CDO = CastChecked<UNTTNiagaraTextRendererProperties>(UNTTNiagaraTextRendererProperties::StaticClass()->GetDefaultObject());
	CDO->InitBindings();

	for (TWeakObjectPtr<UNTTNiagaraTextRendererProperties>& WeakFontRendererProperties : FontRendererPropertiesToDeferredInit)
	{
		if (WeakFontRendererProperties.Get())
		{
			WeakFontRendererProperties->InitBindings();
		}
	}
}

void UNTTNiagaraTextRendererProperties::InitBindings()
{
	if (PositionBinding.GetParamMapBindableVariable().GetName() == NAME_None)
	{
		PositionBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_POSITION);
		ColorBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_COLOR);
		VelocityBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_VELOCITY);
		SpriteRotationBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		SpriteSizeBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_SIZE);
		SpriteFacingBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_FACING);
		SpriteAlignmentBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		DynamicMaterialBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		DynamicMaterial1Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1);
		DynamicMaterial2Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2);
		DynamicMaterial3Binding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3);
		CameraOffsetBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_CAMERA_OFFSET);
		UVScaleBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_UV_SCALE);
		PivotOffsetBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_PIVOT_OFFSET);
		MaterialRandomBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_MATERIAL_RANDOM);
		NormalizedAgeBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		CharacterIndexBinding = FNiagaraConstants::GetAttributeDefaultBinding(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.NTT_CharacterIndex")));
		//CharacterIndexBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_UNIQUE_ID);

		//Default custom sorting to age
		CustomSortingBinding = FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
	}

	SetPreviousBindings(FVersionedNiagaraEmitter(), SourceMode);
}

void UNTTNiagaraTextRendererProperties::SetPreviousBindings(const FVersionedNiagaraEmitter& SrcEmitter, ENiagaraRendererSourceDataMode InSourceMode)
{
	PrevPositionBinding.SetAsPreviousValue(PositionBinding, SrcEmitter, InSourceMode);
	PrevVelocityBinding.SetAsPreviousValue(VelocityBinding, SrcEmitter, InSourceMode);
	PrevSpriteRotationBinding.SetAsPreviousValue(SpriteRotationBinding, SrcEmitter, InSourceMode);
	PrevSpriteSizeBinding.SetAsPreviousValue(SpriteSizeBinding, SrcEmitter, InSourceMode);
	PrevSpriteFacingBinding.SetAsPreviousValue(SpriteFacingBinding, SrcEmitter, InSourceMode);
	PrevSpriteAlignmentBinding.SetAsPreviousValue(SpriteAlignmentBinding, SrcEmitter, InSourceMode);
	PrevCameraOffsetBinding.SetAsPreviousValue(CameraOffsetBinding, SrcEmitter, InSourceMode);
	PrevPivotOffsetBinding.SetAsPreviousValue(PivotOffsetBinding, SrcEmitter, InSourceMode);
}

void UNTTNiagaraTextRendererProperties::CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData)
{
	UpdateSourceModeDerivates(SourceMode);
	UpdateMICs();

	// Initialize layout
	const int32 NumLayoutVars = NeedsPreciseMotionVectors() ? ENTTNiagaraSpriteVFLayout::Num_Max : ENTTNiagaraSpriteVFLayout::Num_Default;
	RendererLayoutWithCustomSort.Initialize(NumLayoutVars);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PositionBinding, ENTTNiagaraSpriteVFLayout::Position);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, VelocityBinding, ENTTNiagaraSpriteVFLayout::Velocity);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, ColorBinding, ENTTNiagaraSpriteVFLayout::Color);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteRotationBinding, ENTTNiagaraSpriteVFLayout::Rotation);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteSizeBinding, ENTTNiagaraSpriteVFLayout::Size);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteFacingBinding, ENTTNiagaraSpriteVFLayout::Facing);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, SpriteAlignmentBinding, ENTTNiagaraSpriteVFLayout::Alignment);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, CameraOffsetBinding, ENTTNiagaraSpriteVFLayout::CameraOffset);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, UVScaleBinding, ENTTNiagaraSpriteVFLayout::UVScale);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PivotOffsetBinding, ENTTNiagaraSpriteVFLayout::PivotOffset);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, NormalizedAgeBinding, ENTTNiagaraSpriteVFLayout::NormalizedAge);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, CharacterIndexBinding, ENTTNiagaraSpriteVFLayout::CharacterIndex);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, MaterialRandomBinding, ENTTNiagaraSpriteVFLayout::MaterialRandom);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, CustomSortingBinding, ENTTNiagaraSpriteVFLayout::CustomSorting);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterialBinding, ENTTNiagaraSpriteVFLayout::MaterialParam0);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial1Binding, ENTTNiagaraSpriteVFLayout::MaterialParam1);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial2Binding, ENTTNiagaraSpriteVFLayout::MaterialParam2);
	RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial3Binding, ENTTNiagaraSpriteVFLayout::MaterialParam3);
	if (NeedsPreciseMotionVectors())
	{
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevPositionBinding, ENTTNiagaraSpriteVFLayout::PrevPosition);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevVelocityBinding, ENTTNiagaraSpriteVFLayout::PrevVelocity);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteRotationBinding, ENTTNiagaraSpriteVFLayout::PrevRotation);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteSizeBinding, ENTTNiagaraSpriteVFLayout::PrevSize);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteFacingBinding, ENTTNiagaraSpriteVFLayout::PrevFacing);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteAlignmentBinding, ENTTNiagaraSpriteVFLayout::PrevAlignment);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevCameraOffsetBinding, ENTTNiagaraSpriteVFLayout::PrevCameraOffset);
		RendererLayoutWithCustomSort.SetVariableFromBinding(CompiledData, PrevPivotOffsetBinding, ENTTNiagaraSpriteVFLayout::PrevPivotOffset);
	}
	RendererLayoutWithCustomSort.Finalize();

	RendererLayoutWithoutCustomSort.Initialize(NumLayoutVars);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PositionBinding, ENTTNiagaraSpriteVFLayout::Position);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, VelocityBinding, ENTTNiagaraSpriteVFLayout::Velocity);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, ColorBinding, ENTTNiagaraSpriteVFLayout::Color);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteRotationBinding, ENTTNiagaraSpriteVFLayout::Rotation);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteSizeBinding, ENTTNiagaraSpriteVFLayout::Size);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteFacingBinding, ENTTNiagaraSpriteVFLayout::Facing);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, SpriteAlignmentBinding, ENTTNiagaraSpriteVFLayout::Alignment);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, CameraOffsetBinding, ENTTNiagaraSpriteVFLayout::CameraOffset);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, UVScaleBinding, ENTTNiagaraSpriteVFLayout::UVScale);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PivotOffsetBinding, ENTTNiagaraSpriteVFLayout::PivotOffset);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, NormalizedAgeBinding, ENTTNiagaraSpriteVFLayout::NormalizedAge);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, CharacterIndexBinding, ENTTNiagaraSpriteVFLayout::CharacterIndex);
	RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, MaterialRandomBinding, ENTTNiagaraSpriteVFLayout::MaterialRandom);
	const bool bDynamicParam0Valid = RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterialBinding,  ENTTNiagaraSpriteVFLayout::MaterialParam0);
	const bool bDynamicParam1Valid = RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial1Binding, ENTTNiagaraSpriteVFLayout::MaterialParam1);
	const bool bDynamicParam2Valid = RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial2Binding, ENTTNiagaraSpriteVFLayout::MaterialParam2);
	const bool bDynamicParam3Valid = RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, DynamicMaterial3Binding, ENTTNiagaraSpriteVFLayout::MaterialParam3);
	if (NeedsPreciseMotionVectors())
	{
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevPositionBinding, ENTTNiagaraSpriteVFLayout::PrevPosition);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevVelocityBinding, ENTTNiagaraSpriteVFLayout::PrevVelocity);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteRotationBinding, ENTTNiagaraSpriteVFLayout::PrevRotation);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteSizeBinding, ENTTNiagaraSpriteVFLayout::PrevSize);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteFacingBinding, ENTTNiagaraSpriteVFLayout::PrevFacing);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevSpriteAlignmentBinding, ENTTNiagaraSpriteVFLayout::PrevAlignment);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevCameraOffsetBinding, ENTTNiagaraSpriteVFLayout::PrevCameraOffset);
		RendererLayoutWithoutCustomSort.SetVariableFromBinding(CompiledData, PrevPivotOffsetBinding, ENTTNiagaraSpriteVFLayout::PrevPivotOffset);
	}
	RendererLayoutWithoutCustomSort.Finalize();

#if WITH_EDITORONLY_DATA
	// Build dynamic parameter mask
	// Serialize in cooked builds
	const FVersionedNiagaraEmitterData* EmitterData = GetEmitterData();
	MaterialParamValidMask  = bDynamicParam0Valid ? GetDynamicParameterChannelMask(EmitterData, DynamicMaterialBinding.GetName(),  0xf) <<  0 : 0;
	MaterialParamValidMask |= bDynamicParam1Valid ? GetDynamicParameterChannelMask(EmitterData, DynamicMaterial1Binding.GetName(), 0xf) <<  4 : 0;
	MaterialParamValidMask |= bDynamicParam2Valid ? GetDynamicParameterChannelMask(EmitterData, DynamicMaterial2Binding.GetName(), 0xf) <<  8 : 0;
	MaterialParamValidMask |= bDynamicParam3Valid ? GetDynamicParameterChannelMask(EmitterData, DynamicMaterial3Binding.GetName(), 0xf) << 12 : 0;
#endif
}

#if WITH_EDITORONLY_DATA
TArray<FNiagaraVariable> UNTTNiagaraTextRendererProperties::GetBoundAttributes() const
{
	TArray<FNiagaraVariable> BoundAttributes = Super::GetBoundAttributes();
	BoundAttributes.Reserve(BoundAttributes.Num() + MaterialParameters.AttributeBindings.Num());

	for (const FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
	{
		BoundAttributes.AddUnique(MaterialParamBinding.GetParamMapBindableVariable());
	}
	return BoundAttributes;
}
#endif

bool UNTTNiagaraTextRendererProperties::PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore)
{
	bool bAnyAdded = Super::PopulateRequiredBindings(InParameterStore);

	for (const FNiagaraVariableAttributeBinding* Binding : AttributeBindings)
	{
		if (Binding && Binding->CanBindToHostParameterMap())
		{
			InParameterStore.AddParameter(Binding->GetParamMapBindableVariable(), false);
			bAnyAdded = true;
		}
	}

	for (FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
	{
		InParameterStore.AddParameter(MaterialParamBinding.GetParamMapBindableVariable(), false);
		bAnyAdded = true;
	}

	return bAnyAdded;
}

void UNTTNiagaraTextRendererProperties::UpdateSourceModeDerivates(ENiagaraRendererSourceDataMode InSourceMode, bool bFromPropertyEdit)
{
	Super::UpdateSourceModeDerivates(InSourceMode, bFromPropertyEdit);
	
	UNiagaraEmitter* SrcEmitter = GetTypedOuter<UNiagaraEmitter>();
	if (SrcEmitter)
	{
		for (FNiagaraMaterialAttributeBinding& MaterialParamBinding : MaterialParameters.AttributeBindings)
		{
			MaterialParamBinding.CacheValues(SrcEmitter);
		}

		SetPreviousBindings(FVersionedNiagaraEmitter(), InSourceMode);
	}
}

void UNTTNiagaraTextRendererProperties::UpdateMICs()
{
#if WITH_EDITORONLY_DATA
	UpdateMaterialParametersMIC(MaterialParameters, Material, MICMaterial);
#endif
}

#if WITH_EDITORONLY_DATA
void UNTTNiagaraTextRendererProperties::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();

	// Update our MICs if we change material / material bindings
	//-OPT: Could narrow down further to only static materials
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UNTTNiagaraTextRendererProperties, Material)) ||
		(MemberPropertyName == GET_MEMBER_NAME_CHECKED(UNTTNiagaraTextRendererProperties, MaterialParameters)) )
	{
		UpdateMICs();
	}

	// If changing the source mode, we may need to update many of our values.
	if (PropertyChangedEvent.GetPropertyName() == TEXT("SourceMode"))
	{
		UpdateSourceModeDerivates(SourceMode, true);
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(PropertyChangedEvent.Property))
	{
		if (StructProp->Struct == FNiagaraVariableAttributeBinding::StaticStruct())
		{
			UpdateSourceModeDerivates(SourceMode, true);
		}
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(PropertyChangedEvent.Property))
	{
		if (ArrayProp->Inner)
		{
			FStructProperty* ChildStructProp = CastField<FStructProperty>(ArrayProp->Inner);
			if (ChildStructProp->Struct == FNiagaraMaterialAttributeBinding::StaticStruct())
			{
				UpdateSourceModeDerivates(SourceMode, true);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UNTTNiagaraTextRendererProperties::RenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, const FVersionedNiagaraEmitter& InEmitter)
{
	Super::RenameVariable(OldVariable, NewVariable, InEmitter);
#if WITH_EDITORONLY_DATA
	MaterialParameters.RenameVariable(OldVariable, NewVariable, InEmitter, GetCurrentSourceMode());
#endif
}

void UNTTNiagaraTextRendererProperties::RemoveVariable(const FNiagaraVariableBase& OldVariable, const FVersionedNiagaraEmitter& InEmitter)
{
	Super::RemoveVariable(OldVariable, InEmitter);
#if WITH_EDITORONLY_DATA
	MaterialParameters.RemoveVariable(OldVariable, InEmitter, GetCurrentSourceMode());
#endif
}

#if WITH_EDITOR
const FSlateBrush* UNTTNiagaraTextRendererProperties::GetStackIcon() const
{
	const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle("NiagaraTextToolkitStyle");
	if (Style)
	{
		return Style->GetBrush("NiagaraTextToolkit.TextRendererIcon");
	}
	return nullptr;
}
#endif

const TArray<FNiagaraVariable>& UNTTNiagaraTextRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(SYS_PARAM_PARTICLES_POSITION);
		Attrs.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attrs.Add(SYS_PARAM_PARTICLES_COLOR);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attrs.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_FACING);
		Attrs.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2);
		Attrs.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3);
		Attrs.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET);
		Attrs.Add(SYS_PARAM_PARTICLES_UV_SCALE);
		Attrs.Add(SYS_PARAM_PARTICLES_PIVOT_OFFSET);
		Attrs.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM);
	}

	return Attrs;
}

void UNTTNiagaraTextRendererProperties::GetAdditionalVariables(TArray<FNiagaraVariableBase>& OutArray) const
{
	if (NeedsPreciseMotionVectors())
	{
		OutArray.Reserve(8);
		OutArray.Add(PrevPositionBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevVelocityBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteRotationBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteSizeBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteFacingBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevSpriteAlignmentBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevCameraOffsetBinding.GetParamMapBindableVariable());
		OutArray.Add(PrevPivotOffsetBinding.GetParamMapBindableVariable());		
	}
}

void UNTTNiagaraTextRendererProperties::GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TSharedRef<SWidget> ThumbnailWidget = SNullWidget::NullWidget;
	int32 ThumbnailSize = 32;
	TArray<UMaterialInterface*> Materials;
	GetUsedMaterials(InEmitter, Materials);
	for (UMaterialInterface* PreviewedMaterial : Materials)
	{
		TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(PreviewedMaterial, ThumbnailSize, ThumbnailSize, InThumbnailPool));
		if (AssetThumbnail)
		{
			ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
		}
		OutWidgets.Add(ThumbnailWidget);
	}

	if (Materials.Num() == 0)
	{
		TSharedRef<SWidget> SpriteWidget = SNew(SImage)
			.Image(FSlateIconFinder::FindIconBrushForClass(GetClass()));
		OutWidgets.Add(SpriteWidget);
	}
}


void UNTTNiagaraTextRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FText>& OutErrors, TArray<FText>& OutWarnings, TArray<FText>& OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);
}

void UNTTNiagaraTextRendererProperties::GetRendererFeedback(const FVersionedNiagaraEmitter & InEmitter, TArray<FNiagaraRendererFeedback>&OutErrors, TArray<FNiagaraRendererFeedback>&OutWarnings, TArray<FNiagaraRendererFeedback>&OutInfo) const
{
	Super::GetRendererFeedback(InEmitter, OutErrors, OutWarnings, OutInfo);

	if (MaterialParameters.HasAnyBindings())
	{
		TArray<UMaterialInterface*> Materials;
		GetUsedMaterials(nullptr, Materials);
		MaterialParameters.GetFeedback(Materials, OutWarnings);
	}
}

void UNTTNiagaraTextRendererProperties::GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const
{
	TArray<UMaterialInterface*> Materials;
	GetUsedMaterials(InEmitter, Materials);
	if (Materials.Num() > 0)
	{
		GetRendererWidgets(InEmitter, OutWidgets, InThumbnailPool);
	}
	else
	{
		TSharedRef<SWidget> SpriteTooltip = SNew(STextBlock)
			.Text(LOCTEXT("SpriteRendererNoMat", "Sprite Renderer (No Material Set)"));
		OutWidgets.Add(SpriteTooltip);
	}
}

FNiagaraVariable UNTTNiagaraTextRendererProperties::GetBoundAttribute(const FNiagaraVariableAttributeBinding* Binding) const
{
	if (!NeedsPreciseMotionVectors())
	{
		if (Binding == &PrevPositionBinding
			|| Binding == &PrevVelocityBinding
			|| Binding == &PrevSpriteRotationBinding
			|| Binding == &PrevSpriteSizeBinding
			|| Binding == &PrevSpriteFacingBinding
			|| Binding == &PrevSpriteAlignmentBinding
			|| Binding == &PrevCameraOffsetBinding
			|| Binding == &PrevPivotOffsetBinding)
		{
			return FNiagaraVariable();
		}
	}

	return Super::GetBoundAttribute(Binding);
}

#endif // WITH_EDITORONLY_DATA

