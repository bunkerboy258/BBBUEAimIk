#include "AimIKResponse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKPoseIndependenceTest, "BBBAimIK.Response.PoseIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKPoseIndependenceTest::RunTest(const FString& Parameters)
{
    FAimIKResponse Response;
    FAimIKResponse Reference;
    const FVector Target = FVector::ForwardVector;
    Response.Update(Target, Target, FVector::UpVector, FVector::ForwardVector, 18.0f, 0.0f, 180.0f, 180.0f);
    Reference.Update(Target, Target, FVector::UpVector, FVector::ForwardVector, 18.0f, 0.0f, 180.0f, 180.0f);
    const FVector Kicked = FRotator(6.0f, -2.0f, 0.0f).Vector();
    const FVector First = Response.Update(Kicked, Target, FVector::UpVector, FVector::ForwardVector,
        18.0f, 1.0f / 60.0f, 180.0f, 180.0f);
    TestTrue(TEXT("固定目标下输入姿势变化不带动追随方向"), First.Equals(Target, 1.e-8));
    const FVector Repeated = Response.Update(Kicked, Target, FVector::UpVector, FVector::ForwardVector,
        18.0f, 0.0f, 180.0f, 180.0f);
    TestTrue(TEXT("同帧再次求值不重复积分"), Repeated.Equals(First, 1.e-8));
    const FVector MovingTarget = FRotator(20.0f, 35.0f, 0.0f).Vector();
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        const FVector Pose = FRotator(Frame * 3.0f, Frame * 7.0f, 0.0f).Vector();
        const FVector Result = Response.Update(Pose, MovingTarget, FVector::UpVector, FVector::ForwardVector,
            18.0f, 1.0f / 60.0f, 180.0f, 180.0f);
        const FVector Expected = Reference.Update(Target, MovingTarget, FVector::UpVector, FVector::ForwardVector,
            18.0f, 1.0f / 60.0f, 180.0f, 180.0f);
        TestTrue(TEXT("目标追随过程不受输入姿势变化影响"), Result.Equals(Expected, 1.e-8));
    }
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
        Response.Update(Input, Target, FVector::UpVector, FVector::ForwardVector,
            6.0f, 0.0f, 180.0f, 180.0f);
        FVector Result = Input;
        for (int32 Frame = 0; Frame < Rate; ++Frame)
        {
            Result = Response.Update(Input, Target, FVector::UpVector, FVector::ForwardVector,
                6.0f, 1.0f / Rate, 180.0f, 180.0f);
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
    const FVector First = Response.Update(Input, Target, FVector::UpVector, FVector::ForwardVector,
        5.0f, 1.0f / 60.0f, 180.0f, 180.0f);
    TestTrue(TEXT("移动目标不会瞬间吸附"), !First.Equals(Target, 1.e-3));
    TestTrue(TEXT("移动目标后朝正确方向纠正"), First.Y > 0.0 && First.Z > 0.0);
    Response.Reset();
    const FVector Restored = Response.Update(FVector::UpVector, Target, FVector::UpVector, FVector::ForwardVector,
        5.0f, 0.0f, 180.0f, 180.0f);
    TestTrue(TEXT("重新启用从当前输入姿势开始"), Restored.Equals(FVector::UpVector));
    const FVector Opposite = Response.Update(-FVector::UpVector, -Target, FVector::UpVector, FVector::ForwardVector,
        5.0f, 0.2f, 180.0f, 180.0f);
    TestTrue(TEXT("反向与极点附近结果有限且归一化"), !Opposite.ContainsNaN() && Opposite.IsNormalized());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKRearTargetContinuityTest, "BBBAimIK.Response.RearTargetContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKRearTargetContinuityTest::RunTest(const FString& Parameters)
{
    FAimIKResponse Response;
    const FVector Input = FVector::ForwardVector;
    const FVector RightRear = FRotator(0.0f, 179.0f, 0.0f).Vector();
    const FVector LeftRear = FRotator(0.0f, -179.0f, 0.0f).Vector();
    FVector Previous = Input;
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Previous = Response.Update(Input, RightRear, FVector::UpVector, FVector::ForwardVector,
            18.0f, 1.0f / 60.0f, 60.0f, 90.0f);
    }

    const FVector Current = Response.Update(Input, LeftRear, FVector::UpVector, FVector::ForwardVector,
        18.0f, 1.0f / 60.0f, 60.0f, 90.0f);
    const float ChangeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(Previous, Current), -1.0f, 1.0f)));
    const float CorrectionDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(Input, Current), -1.0f, 1.0f)));
    TestTrue(TEXT("目标越过正后方时不会从水平限制一侧跳到另一侧"), ChangeDegrees < 15.0f);
    TestTrue(TEXT("目标越过正后方时仍遵守六十度水平上限"), CorrectionDegrees <= 60.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKIndependentAngleLimitsTest, "BBBAimIK.Response.IndependentAngleLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKIndependentAngleLimitsTest::RunTest(const FString& Parameters)
{
    const FVector Input = FVector::ForwardVector;
    const FVector HorizontalTarget = FRotator(40.0f, 100.0f, 0.0f).Vector();
    const FVector HorizontalLimited = FAimIKAngleLimit::ClampDirection(
        Input, HorizontalTarget, FVector::UpVector, FVector::ForwardVector,
        60.0f, 90.0f, Input);
    const FRotator HorizontalResult = HorizontalLimited.Rotation();
    TestTrue(TEXT("水平超限只截断到六十度"), FMath::IsNearlyEqual(HorizontalResult.Yaw, 60.0f, 0.01f));
    TestTrue(TEXT("水平截断保留原有垂直角"), FMath::IsNearlyEqual(HorizontalResult.Pitch, 40.0f, 0.01f));

    const FVector VerticalTarget = FRotator(80.0f, 30.0f, 0.0f).Vector();
    const FVector VerticalLimited = FAimIKAngleLimit::ClampDirection(
        Input, VerticalTarget, FVector::UpVector, FVector::ForwardVector,
        60.0f, 45.0f, Input);
    const FRotator VerticalResult = VerticalLimited.Rotation();
    TestTrue(TEXT("垂直超限只截断到四十五度"), FMath::IsNearlyEqual(VerticalResult.Pitch, 45.0f, 0.01f));
    TestTrue(TEXT("垂直截断保留原有水平角"), FMath::IsNearlyEqual(VerticalResult.Yaw, 30.0f, 0.01f));

    const FVector PitchedInput = FRotator(30.0f, 0.0f, 0.0f).Vector();
    const FVector BelowInput = FRotator(-80.0f, 0.0f, 0.0f).Vector();
    const FVector PitchedLimited = FAimIKAngleLimit::ClampDirection(
        PitchedInput, BelowInput, FVector::UpVector, FVector::ForwardVector,
        180.0f, 90.0f, PitchedInput);
    TestTrue(TEXT("垂直上限相对当前输入姿态计算"),
        FMath::IsNearlyEqual(PitchedLimited.Rotation().Pitch, -60.0f, 0.01f));
    return true;
}
#endif
