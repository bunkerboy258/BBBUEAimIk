#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * BBBAimIK 运行时模块入口
 */
class FBBBAimIKModule : public IModuleInterface
{
public:
    //~ Begin IModuleInterface Interface
    virtual void StartupModule() override
    {
    }

    virtual void ShutdownModule() override
    {
    }
    //~ End IModuleInterface Interface
};

IMPLEMENT_MODULE(FBBBAimIKModule, BBBAimIK)
