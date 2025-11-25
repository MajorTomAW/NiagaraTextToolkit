// Copyright Epic Games, Inc. All Rights Reserved.

// NiagaraTextParticles.cpp

#include "NiagaraTextParticles.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FNiagaraTextParticlesModule"

void FNiagaraTextParticlesModule::StartupModule()
{
    // No shader mapping here anymore – handled by NiagaraTextParticlesVertexFactories module.
}

void FNiagaraTextParticlesModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNiagaraTextParticlesModule, NiagaraTextParticles)