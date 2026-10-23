#include "AimIKResponse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKPoseDisturbanceTest, "BBBAimIK.Response.PoseDisturbance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKPoseDisturbanceTest::RunTest(const FString& Parameters)
{
    FAimIKResponse Response;
    const FVector Target = FVector::ForwardVector;
    Response.Update(Target, Target, 18.0f, 0.0f);
    const FVector Kicked = FRotator(6.0f, -2.0f, 0.0f).Vector();
    const FVector First = Response.Update(Kicked, Target, 18.0f, 1.0f / 60.0f);
    TestTrue(TEXT("固定目标下输入姿势扰动不会被当帧消除"), First.Z > 0.01 && First.Y < -0.001);
    TestTrue(TEXT("纠正后误差小于原始扰动"), FVector::DotProduct(First, Target) > FVector::DotProduct(Kicked, Target));
    const FVector Repeated = Response.Update(Kicked, Target, 18.0f, 0.0f);
    TestTrue(TEXT("同帧再次求值不重复积分"), Repeated.Equals(First, 1.e-8));
    FVector Result = First;
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Result = Response.Update(Kicked, Target, 18.0f, 1.0f / 60.0f);
    }
    TestTrue(TEXT("保持相同输入姿势时持续纠正到真实目标"), Result.Equals(Target, 1.e-6));
    TestTrue(TEXT("输入目标没有被修改"), Target.Equals(FVector::ForwardVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKResponseFrameRateTest, "BBBAimIK.Response.FrameRate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKResponseFrameRateTest::RunTest(const FString& Parameters)
{
    FVector Reference = FVector::ZeroVector;
    for (const int32 Rate : {30, 60, 120})
    {
        FAimIKResponse Response;
        const FVector Target = FVector::ForwardVector;
        const FVector Input = FRotator(12.0f, 8.0f, 0.0f).Vector();
        Response.Update(Target, Target, 6.0f, 0.0f);
        FVector Result = Input;
        for (int32 Frame = 0; Frame < Rate; ++Frame)
        {
            Result = Response.Update(Input, Target, 6.0f, 1.0f / Rate);
        }
        if (Reference.IsNearlyZero())
        {
            Reference = Result;
        }
        TestTrue(FString::Printf(TEXT("%d 帧每秒下等时长纠正一致"), Rate), Result.Equals(Reference, 1.e-5));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKResponseResetTest, "BBBAimIK.Response.ResetAndTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKResponseResetTest::RunTest(const FString& Parameters)
{
    FAimIKResponse Response;
    const FVector Input = FVector::ForwardVector;
    const FVector Target = FRotator(20.0f, 35.0f, 0.0f).Vector();
    const FVector First = Response.Update(Input, Target, 5.0f, 1.0f / 60.0f);
    TestTrue(TEXT("移动目标不会瞬间吸附"), !First.Equals(Target, 1.e-3));
    TestTrue(TEXT("移动目标后朝正确方向纠正"), First.Y > 0.0 && First.Z > 0.0);
    Response.Reset();
    const FVector Restored = Response.Update(FVector::UpVector, Target, 5.0f, 0.0f);
    TestTrue(TEXT("重新启用从当前输入姿势开始"), Restored.Equals(FVector::UpVector));
    const FVector Opposite = Response.Update(-FVector::UpVector, -Target, 5.0f, 0.2f);
    TestTrue(TEXT("反向与极点附近结果有限且归一化"), !Opposite.ContainsNaN() && Opposite.IsNormalized());
    return true;
}
#endif
