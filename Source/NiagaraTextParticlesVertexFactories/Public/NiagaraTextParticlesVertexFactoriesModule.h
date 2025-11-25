#pragma once

#include "Modules/ModuleInterface.h"

class FNiagaraTextParticlesVertexFactoriesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};


