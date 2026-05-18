#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBBBAimIKModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FBBBAimIKModule, BBBAimIK)
