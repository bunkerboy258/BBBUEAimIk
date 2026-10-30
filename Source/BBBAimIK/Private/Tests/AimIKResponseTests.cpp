#include "AimIKResponse.h"
#include "AimIKSolver.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKReferenceValidationTest, "BBBAimIK.Reference.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKReferenceValidationTest::RunTest(const FString& Parameters)
{
    FAimIKReferenceFrame Frame;
    TestFalse(TEXT("拒绝零瞄准轴"), FAimIKReferenceFrame::TryCreate(
        FTransform::Identity, FVector::ZeroVector, FVector::UpVector, Frame));
    TestFalse(TEXT("拒绝共线参考轴"), FAimIKReferenceFrame::TryCreate(
        FTransform::Identity, FVector::ForwardVector, FVector::ForwardVector, Frame));
    TestFalse(TEXT("拒绝非有限参考轴"), FAimIKReferenceFrame::TryCreate(
        FTransform::Identity, FVector::ForwardVector,
        FVector(0.0, 0.0, std::numeric_limits<double>::infinity()), Frame));
    const FTransform Source(FRotator(20.0, 75.0, 90.0), FVector(11.0, 22.0, 33.0));
    TestTrue(TEXT("接受有效瞄准源"), FAimIKReferenceFrame::TryCreate(
        Source, FVector::ForwardVector, FVector(0.2, 0.0, 1.0), Frame));
    TestTrue(TEXT("参考原点来自瞄准源"), Frame.Origin.Equals(Source.GetLocation()));
    TestTrue(TEXT("参考轴正交"), FMath::Abs(FVector::DotProduct(Frame.Forward, Frame.Up)) < 1.e-8);
    TestTrue(TEXT("上轴极点转换有限"), Frame.FromAngles(Frame.ToAngles(Frame.Up, 1.0)).Equals(Frame.Up, 1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKResponseFrameRateTest, "BBBAimIK.Response.FrameRate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKResponseFrameRateTest::RunTest(const FString& Parameters)
{
    const FAimIKReferenceFrame Frame;
    const FVector Target = FRotator(20.0, 35.0, 0.0).Vector();
    FVector Expected = FVector::ZeroVector;
    for (const int32 Rate : {15, 30, 60, 120})
    {
        FAimIKResponse Response;
        FVector Result = Frame.Forward;
        for (int32 Index = 0; Index < Rate; ++Index)
        {
            Result = Response.Update(Frame, Target, 6.0f, 1.0f / Rate, 180.0f, 180.0f);
        }
        if (Expected.IsNearlyZero())
        {
            Expected = Result;
        }
        TestTrue(FString::Printf(TEXT("%d FPS 等时长响应一致"), Rate), Result.Equals(Expected, 1.e-6));
        TestTrue(TEXT("同帧重复求值不推进"), Response.Update(Frame, Target, 6.0f, 0.0f, 180.0f, 180.0f).Equals(Result, 1.e-8));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKRearTargetContinuityTest, "BBBAimIK.Response.RearTargetContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKRearTargetContinuityTest::RunTest(const FString& Parameters)
{
    const FAimIKReferenceFrame Frame;
    FAimIKResponse Response;
    FVector Previous = Frame.Forward;
    for (int32 Index = 0; Index < 180; ++Index)
    {
        const double Yaw = Index < 60 || Index % 2 == 0 ? 179.0 : -179.0;
        const FVector Target = FRotator(0.01, Yaw, 0.0).Vector();
        const FVector Result = Response.Update(Frame, Target, 30.0f, 1.0f / 60.0f, 60.0f, 90.0f);
        const FVector2D Angles = Frame.ToAngles(Result);
        TestTrue(TEXT("近反向微小俯仰不产生向下绕行"), FMath::Abs(Angles.Y) < FMath::DegreesToRadians(0.02));
        TestTrue(TEXT("水平修正服从六十度上限"), FMath::Abs(Angles.X) <= FMath::DegreesToRadians(60.001));
        if (Index > 60)
        {
            TestTrue(TEXT("跨背后不跳转到另一边"), FVector::DotProduct(Previous, Result) > 0.99);
        }
        Previous = Result;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKReferenceRotationTest, "BBBAimIK.Reference.RotationEquivariance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKReferenceRotationTest::RunTest(const FString& Parameters)
{
    const FAimIKReferenceFrame Original;
    const FTransform Rotation(FRotator(65.0, 135.0, 90.0));
    const FAimIKReferenceFrame Rotated = Original.TransformBy(Rotation);
    FAimIKResponse BaseResponse;
    FAimIKResponse RotatedResponse;
    for (int32 Index = 0; Index < 120; ++Index)
    {
        const FVector Target = FRotator(40.0, Index < 60 ? 170.0 : -170.0, 0.0).Vector();
        const FVector Base = BaseResponse.Update(Original, Target, 18.0f, 1.0f / 60.0f, 60.0f, 30.0f);
        const FVector Result = RotatedResponse.Update(Rotated, Rotation.TransformVectorNoScale(Target),
            18.0f, 1.0f / 60.0f, 60.0f, 30.0f);
        TestTrue(TEXT("源与目标整体旋转后响应保持相同相对关系"), Result.Equals(Rotation.TransformVectorNoScale(Base), 1.e-7));
    }
    const FVector Limited = FAimIKAngleLimit::ClampDirection(Rotated,
        Rotated.FromAngles(FVector2D(FMath::DegreesToRadians(100.0), FMath::DegreesToRadians(70.0))),
        60.0f, 30.0f, Rotated.Forward);
    const FVector2D Angles = Rotated.ToAngles(Limited);
    TestTrue(TEXT("侧倾后水平限制跟随源参考系"), FMath::IsNearlyEqual(Angles.X, FMath::DegreesToRadians(60.0), 1.e-7));
    TestTrue(TEXT("侧倾后垂直限制跟随源参考系"), FMath::IsNearlyEqual(Angles.Y, FMath::DegreesToRadians(30.0), 1.e-7));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKResponseResetTest, "BBBAimIK.Response.ResetAndWorldHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKResponseResetTest::RunTest(const FString& Parameters)
{
    FAimIKResponse Response;
    const FAimIKReferenceFrame Original;
    const FVector Target = FVector::ForwardVector;
    Response.Update(Original, Target, 18.0f, 0.0f, 180.0f, 180.0f);
    const FAimIKReferenceFrame Changed = Original.TransformBy(FTransform(FRotator(10.0, 150.0, 40.0)));
    const FVector Result = Response.Update(Changed, Target, 18.0f, 1.0f / 60.0f, 180.0f, 180.0f);
    TestTrue(TEXT("固定世界目标不因输入骨骼旋转而重复转动"), Result.Equals(Target, 1.e-7));
    Response.Reset();
    const FVector Reset = Response.Update(Changed, Target, 18.0f, 0.0f, 180.0f, 180.0f);
    TestTrue(TEXT("重启从当前源前向开始"), Reset.Equals(Changed.Forward, 1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimIKSolverReferenceTest, "BBBAimIK.Solver.ReferenceTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAimIKSolverReferenceTest::RunTest(const FString& Parameters)
{
    TArray<FAimIKBoneRef> Chain;
    Chain.AddDefaulted();
    const FAimIKReferenceFrame Original;
    const FTransform Rotation(FRotator(20.0, 80.0, 90.0));
    const FAimIKReferenceFrame Frame = Original.TransformBy(Rotation);
    FAimIKSolverInput Input(Chain);
    Input.ReferenceFrameCS = Frame;
    Input.AimTargetCS = Frame.Origin + Frame.FromAngles(FVector2D(FMath::DegreesToRadians(170.0), 0.0)) * 1000.0;
    Input.ClampWeight = 0.1f;
    Input.ClampSmoothing = 2;
    Input.MaxHorizontalAimCorrectionAngleDegrees = 60.0f;
    Input.MaxVerticalAimCorrectionAngleDegrees = 30.0f;
    TArray<FTransform> Transforms;
    Transforms.Add(Rotation);
    FTransform Aim = Rotation;
    const FVector Effective = FAimIKSolver::Solve(Input, Transforms, Aim);
    const FVector2D Angles = Frame.ToAngles(Effective - Frame.Origin);
    TestTrue(TEXT("求解器软限制不重新引入垂直绕行"), FMath::Abs(Angles.Y) < 1.e-7);
    TestTrue(TEXT("求解器复用相同水平上限"), FMath::Abs(Angles.X) <= FMath::DegreesToRadians(60.001));
    return true;
}
#endif
