// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraTextParticlesEditor.h"
#include "NiagaraSettings.h"
#include "NiagaraEditorModule.h"
#include "NiagaraRendererProperties.h"
#include "NTPNiagaraFontRendererProperties.h"

#define LOCTEXT_NAMESPACE "FNiagaraTextParticlesEditorModule"

void FNiagaraTextParticlesEditorModule::StartupModule()
{
	// Editor-only startup logic for NiagaraTextParticles

	// Register ESpawnTextParticleMode as a Niagara Additional Parameter Enum
	if (UNiagaraSettings* NiagaraSettings = GetMutableDefault<UNiagaraSettings>())
	{
		const FSoftObjectPath EnumPath(TEXT("/NiagaraTextParticles/Enums/ESpawnTextParticleMode.ESpawnTextParticleMode"));

		if (!NiagaraSettings->AdditionalParameterEnums.Contains(EnumPath))
		{
			NiagaraSettings->AdditionalParameterEnums.Add(EnumPath);
			NiagaraSettings->SaveConfig();
		}
	}

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");

	FNiagaraRendererCreationInfo RendererInfo;
	RendererInfo.DisplayName = NSLOCTEXT("Niagara", "NTPFontRenderer", "Font Renderer");
	RendererInfo.Description = NSLOCTEXT("Niagara", "NTPFontRendererDesc", "Font renderer specialized for Niagara Text Particles.");
	RendererInfo.RendererClassPath = FTopLevelAssetPath(TEXT("/Script/NiagaraTextParticles.NTPNiagaraFontRendererProperties"));
	RendererInfo.RendererFactory = FNiagaraRendererCreationInfo::FRendererFactory::CreateLambda(
		[](UObject* Outer) -> UNiagaraRendererProperties*
		{
			return NewObject<UNTPNiagaraFontRendererProperties>(Outer ? Outer : GetTransientPackage());
		}
	);

	NiagaraEditorModule.RegisterRendererCreationInfo(RendererInfo);
}

void FNiagaraTextParticlesEditorModule::ShutdownModule()
{
	// Editor-only shutdown logic for NiagaraTextParticles can be added here
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNiagaraTextParticlesEditorModule, NiagaraTextParticlesEditor)


