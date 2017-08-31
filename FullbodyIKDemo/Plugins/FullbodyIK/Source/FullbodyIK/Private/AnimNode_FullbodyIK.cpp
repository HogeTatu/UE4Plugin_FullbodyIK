#include "AnimNode_FullbodyIK.h"
#include "Engine/Engine.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "SceneManagement.h"
#include "AnimInstanceInterface_FullbodyIK.h"
#include "DrawDebugHelpers.h"

DECLARE_CYCLE_STAT(TEXT("FullbodyIK Eval"), STAT_FullbodyIK_Eval, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixInverse"), STAT_FullbodyIK_MatrixInverse, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixTranspose"), STAT_FullbodyIK_MatrixTranspose, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixMultiply"), STAT_FullbodyIK_MatrixMultiply, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK MatrixDet"), STAT_FullbodyIK_MatrixDet, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK Loop"), STAT_FullbodyIK_Loop, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK CulcEta"), STAT_FullbodyIK_CulcEta, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK CalcJacobian"), STAT_FullbodyIK_CalcJacobian, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK SolveSolver"), STAT_FullbodyIK_SolveSolver, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("FullbodyIK UpdateCenterOfMass"), STAT_FullbodyIK_UpdateCenterOfMass, STATGROUP_Anim);

/////////////////////////////////////////////////////

static const int32 AXIS_COUNT = 3;

static FORCEINLINE float SinD(float X) { return FMath::Sin(FMath::DegreesToRadians(X)); }
static FORCEINLINE float CosD(float X) { return FMath::Cos(FMath::DegreesToRadians(X)); }

static FORCEINLINE FMatrix RotX(float Roll)
{
	return FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, CosD(Roll), -SinD(Roll), 0),
		FPlane(0, SinD(Roll), CosD(Roll), 0),
		FPlane(0, 0, 0, 1)
	);
}
static FORCEINLINE FMatrix RotY(float Pitch)
{
	return FMatrix(
		FPlane(CosD(Pitch), 0, SinD(Pitch), 0),
		FPlane(0, 1, 0, 0),
		FPlane(-SinD(Pitch), 0, CosD(Pitch), 0),
		FPlane(0, 0, 0, 1)
	);
}
static FORCEINLINE FMatrix RotZ(float Yaw)
{
	return FMatrix(
		FPlane(CosD(Yaw), SinD(Yaw), 0, 0),
		FPlane(-SinD(Yaw), CosD(Yaw), 0, 0),
		FPlane(0, 0, 1, 0),
		FPlane(0, 0, 0, 1)
	);
}

static FORCEINLINE FMatrix DiffX(float Roll)
{
	return FMatrix(
		FPlane(0, 0, 0, 0),
		FPlane(0, -SinD(Roll), -CosD(Roll), 0),
		FPlane(0, CosD(Roll), -SinD(Roll), 0),
		FPlane(0, 0, 0, 0)
	);
}
static FORCEINLINE FMatrix DiffY(float Pitch)
{
	return FMatrix(
		FPlane(-SinD(Pitch), 0, CosD(Pitch), 0),
		FPlane(0, 0, 0, 0),
		FPlane(-CosD(Pitch), 0, -SinD(Pitch), 0),
		FPlane(0, 0, 0, 0)
	);
}
static FORCEINLINE FMatrix DiffZ(float Yaw)
{
	return FMatrix(
		FPlane(-SinD(Yaw), CosD(Yaw), 0, 0),
		FPlane(-CosD(Yaw), -SinD(Yaw), 0, 0),
		FPlane(0, 0, 0, 0),
		FPlane(0, 0, 0, 0)
	);
}

static FORCEINLINE float MatrixInverse3(float* DstMatrix, const float* SrcMatrix)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixInverse);

	float Det =
		  SrcMatrix[0 * 3 + 0] * SrcMatrix[1 * 3 + 1] * SrcMatrix[2 * 3 + 2]
		+ SrcMatrix[1 * 3 + 0] * SrcMatrix[2 * 3 + 1] * SrcMatrix[0 * 3 + 2]
		+ SrcMatrix[2 * 3 + 0] * SrcMatrix[0 * 3 + 1] * SrcMatrix[1 * 3 + 2]
		- SrcMatrix[0 * 3 + 0] * SrcMatrix[2 * 3 + 1] * SrcMatrix[1 * 3 + 2]
		- SrcMatrix[2 * 3 + 0] * SrcMatrix[1 * 3 + 1] * SrcMatrix[0 * 3 + 2]
		- SrcMatrix[1 * 3 + 0] * SrcMatrix[0 * 3 + 1] * SrcMatrix[2 * 3 + 2];

	if (Det == 0)
	{
		return Det;
	}

	DstMatrix[0 * 3 + 0] = (SrcMatrix[1 * 3 + 1] * SrcMatrix[2 * 3 + 2] - SrcMatrix[1 * 3 + 2] * SrcMatrix[2 * 3 + 1]) / Det;
	DstMatrix[0 * 3 + 1] = (SrcMatrix[0 * 3 + 2] * SrcMatrix[2 * 3 + 1] - SrcMatrix[0 * 3 + 1] * SrcMatrix[2 * 3 + 2]) / Det;
	DstMatrix[0 * 3 + 2] = (SrcMatrix[0 * 3 + 1] * SrcMatrix[1 * 3 + 2] - SrcMatrix[0 * 3 + 2] * SrcMatrix[1 * 3 + 1]) / Det;

	DstMatrix[1 * 3 + 0] = (SrcMatrix[1 * 3 + 2] * SrcMatrix[2 * 3 + 0] - SrcMatrix[1 * 3 + 0] * SrcMatrix[2 * 3 + 2]) / Det;
	DstMatrix[1 * 3 + 1] = (SrcMatrix[0 * 3 + 0] * SrcMatrix[2 * 3 + 2] - SrcMatrix[0 * 3 + 2] * SrcMatrix[2 * 3 + 0]) / Det;
	DstMatrix[1 * 3 + 2] = (SrcMatrix[0 * 3 + 2] * SrcMatrix[1 * 3 + 0] - SrcMatrix[0 * 3 + 0] * SrcMatrix[1 * 3 + 2]) / Det;

	DstMatrix[2 * 3 + 0] = (SrcMatrix[1 * 3 + 0] * SrcMatrix[2 * 3 + 1] - SrcMatrix[1 * 3 + 1] * SrcMatrix[2 * 3 + 0]) / Det;
	DstMatrix[2 * 3 + 1] = (SrcMatrix[0 * 3 + 1] * SrcMatrix[2 * 3 + 0] - SrcMatrix[0 * 3 + 0] * SrcMatrix[2 * 3 + 1]) / Det;
	DstMatrix[2 * 3 + 2] = (SrcMatrix[0 * 3 + 0] * SrcMatrix[1 * 3 + 1] - SrcMatrix[0 * 3 + 1] * SrcMatrix[1 * 3 + 0]) / Det;

	return Det;
}

static FORCEINLINE float MatrixInverse4(float* DstMatrix, const float* SrcMatrix)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixInverse);

	float Det =
			SrcMatrix[0 * 4 + 0] * (
				SrcMatrix[1 * 4 + 1] * (SrcMatrix[2 * 4 + 2] * SrcMatrix[3 * 4 + 3] - SrcMatrix[2 * 4 + 3] * SrcMatrix[3 * 4 + 2]) -
				SrcMatrix[2 * 4 + 1] * (SrcMatrix[1 * 4 + 2] * SrcMatrix[3 * 4 + 3] - SrcMatrix[1 * 4 + 3] * SrcMatrix[3 * 4 + 2]) +
				SrcMatrix[3 * 4 + 1] * (SrcMatrix[1 * 4 + 2] * SrcMatrix[2 * 4 + 3] - SrcMatrix[1 * 4 + 3] * SrcMatrix[2 * 4 + 2])
				) -
			SrcMatrix[1 * 4 + 0] * (
				SrcMatrix[0 * 4 + 1] * (SrcMatrix[2 * 4 + 2] * SrcMatrix[3 * 4 + 3] - SrcMatrix[2 * 4 + 3] * SrcMatrix[3 * 4 + 2]) -
				SrcMatrix[2 * 4 + 1] * (SrcMatrix[0 * 4 + 2] * SrcMatrix[3 * 4 + 3] - SrcMatrix[0 * 4 + 3] * SrcMatrix[3 * 4 + 2]) +
				SrcMatrix[3 * 4 + 1] * (SrcMatrix[0 * 4 + 2] * SrcMatrix[2 * 4 + 3] - SrcMatrix[0 * 4 + 3] * SrcMatrix[2 * 4 + 2])
				) +
			SrcMatrix[2 * 4 + 0] * (
				SrcMatrix[0 * 4 + 1] * (SrcMatrix[1 * 4 + 2] * SrcMatrix[3 * 4 + 3] - SrcMatrix[1 * 4 + 3] * SrcMatrix[3 * 4 + 2]) -
				SrcMatrix[1 * 4 + 1] * (SrcMatrix[0 * 4 + 2] * SrcMatrix[3 * 4 + 3] - SrcMatrix[0 * 4 + 3] * SrcMatrix[3 * 4 + 2]) +
				SrcMatrix[3 * 4 + 1] * (SrcMatrix[0 * 4 + 2] * SrcMatrix[1 * 4 + 3] - SrcMatrix[0 * 4 + 3] * SrcMatrix[1 * 4 + 2])
				) -
			SrcMatrix[3 * 4 + 0] * (
				SrcMatrix[0 * 4 + 1] * (SrcMatrix[1 * 4 + 2] * SrcMatrix[2 * 4 + 3] - SrcMatrix[1 * 4 + 3] * SrcMatrix[2 * 4 + 2]) -
				SrcMatrix[1 * 4 + 1] * (SrcMatrix[0 * 4 + 2] * SrcMatrix[2 * 4 + 3] - SrcMatrix[0 * 4 + 3] * SrcMatrix[2 * 4 + 2]) +
				SrcMatrix[2 * 4 + 1] * (SrcMatrix[0 * 4 + 2] * SrcMatrix[1 * 4 + 3] - SrcMatrix[0 * 4 + 3] * SrcMatrix[1 * 4 + 2])
				);

	if (Det == 0)
	{
		return Det;
	}

	VectorMatrixInverse(DstMatrix, SrcMatrix);

	return Det;
}

static FORCEINLINE void MatrixTranspose(float* DstMatrix, const float* SrcMatrix, const int32 Row, const int32 Col)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixTranspose);

	for (int32 i = 0; i < Row; ++i)
	{
		for (int32 j = 0; j < Col; ++j)
		{
			DstMatrix[j * Row + i] = SrcMatrix[i * Col + j];
		}
	}
}

static FORCEINLINE void MatrixMultiply(float* DstMatrix, const float* SrcMatrix1, const int32 Row1, const int32 Col1, const float* SrcMatrix2, const int32 Row2, const int32 Col2)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixMultiply);

	check(Col1 == Row2);

	for (int32 i = 0; i < Row1; ++i)
	{
		for (int32 j = 0; j < Col2; ++j)
		{
			float& Elem = DstMatrix[i * Col2 + j];
			Elem = 0;

			for (int32 k = 0; k < Col1; ++k)
			{
				Elem += SrcMatrix1[i * Col1 + k] * SrcMatrix2[k * Col2 + j];
			}
		}
	}
}

static FORCEINLINE float MatrixDet(const float* SrcMatrix, const int32 N)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_MatrixDet);

	float Det = 0;

	for (int32 i = 0; i < N; ++i)
	{
		float AddValue = 1;
		float SubValue = 1;

		for (int32 j = 0; j < N; ++j)
		{
			AddValue *= SrcMatrix[j * N + ((i + j) % N)];
			SubValue *= SrcMatrix[j * N + ((i + (N - j - 1)) % N)];
		}

		Det += AddValue;
		Det -= SubValue;
	}

	return Det;
}

static FORCEINLINE float GetMappedRangeEaseInClamped(
	const float& InRangeMin,
	const float& InRangeMax,
	const float& OutRangeMin,
	const float& OutRangeMax,
	const float& Exp,
	const float& Value)
{
	float Pct = FMath::Clamp((Value - InRangeMin) / (InRangeMax - InRangeMin), 0.f, 1.f);
	return FMath::InterpEaseIn(OutRangeMin, OutRangeMax, Pct, Exp);
}


/////////////////////////////////////////////////////
// FAnimNode_FullbodyIK

FAnimNode_FullbodyIK::FAnimNode_FullbodyIK()
	: Setting(nullptr)
	, EffectorCountMax(4)
	, DebugShowCenterOfMassRadius(0.f)
	, bDebugShowEffectiveCount(false)
{
}

void FAnimNode_FullbodyIK::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (!Setting)
	{
		return;
	}

	const auto Mesh = Context.AnimInstanceProxy->GetSkelMeshComponent();

	for (const auto& IkEndBoneName : IkEndBoneNames)
	{
		FName BoneName = IkEndBoneName;

		while (true)
		{
			int32 BoneIndex = Mesh->GetBoneIndex(BoneName);

			if (BoneIndex == INDEX_NONE || SolverInternals.Contains(BoneIndex))
			{
				break;
			}

			SolverInternals.Add(BoneIndex, FSolverInternal());
			BoneIndices.Add(BoneIndex);

			FName ParentBoneName = Mesh->GetParentBone(BoneName);
			int32 ParentBoneIndex = Mesh->GetBoneIndex(ParentBoneName);
			FFullbodyIKSolver Solver = GetSolver(BoneName);

			FSolverInternal& SolverInternal = SolverInternals[BoneIndex];
			SolverInternal.BoneIndex = BoneIndex;
			SolverInternal.ParentBoneIndex = ParentBoneIndex;
			SolverInternal.BoneIndicesIndex = -1;
			SolverInternal.bTranslation = Solver.bTranslation;
			SolverInternal.bLimited = Solver.bLimited;
			SolverInternal.Mass = Solver.Mass;
			SolverInternal.X = Solver.X;
			SolverInternal.Y = Solver.Y;
			SolverInternal.Z = Solver.Z;

			if (ParentBoneIndex >= 0)
			{
				if (!SolverTree.Contains(ParentBoneIndex))
				{
					SolverTree.Add(ParentBoneIndex, TArray<int32>());
				}

				SolverTree[ParentBoneIndex].Add(BoneIndex);
			}

			BoneName = ParentBoneName;
		}
	}

	BoneIndices.Sort();
	BoneCount = BoneIndices.Num();
	BoneAxisCount = BoneCount * AXIS_COUNT;

	for (int32 i = 0; i < BoneCount; ++i)
	{
		const int32& BoneIndex = BoneIndices[i];
		SolverInternals[BoneIndex].BoneIndicesIndex = i;
	}

	// 最大数で確保
	int32 EffectorCount = EffectorCountMax;
	int32 DisplacementCount = BoneAxisCount;

	ElementsJ.SetNumZeroed(DisplacementCount * AXIS_COUNT);
	ElementsJt.SetNumZeroed(AXIS_COUNT * DisplacementCount);
	ElementsJtJ.SetNumZeroed(AXIS_COUNT * AXIS_COUNT);
	ElementsJtJi.SetNumZeroed(AXIS_COUNT * AXIS_COUNT);
	ElementsJp.SetNumZeroed(AXIS_COUNT * DisplacementCount);
	ElementsW0.SetNumZeroed(BoneAxisCount);
	ElementsWi.SetNumZeroed(DisplacementCount * DisplacementCount);
	ElementsJtWi.SetNumZeroed(AXIS_COUNT * DisplacementCount);
	ElementsJtWiJ.SetNumZeroed(AXIS_COUNT * AXIS_COUNT);
	ElementsJtWiJi.SetNumZeroed(AXIS_COUNT * AXIS_COUNT);
	ElementsJtWiJiJt.SetNumZeroed(AXIS_COUNT * DisplacementCount);
	ElementsJwp.SetNumZeroed(AXIS_COUNT * DisplacementCount);
	ElementsRt1.SetNumZeroed(BoneAxisCount);
	ElementsEta.SetNumZeroed(BoneAxisCount);
	ElementsEtaJ.SetNumZeroed(AXIS_COUNT);
	ElementsEtaJJp.SetNumZeroed(BoneAxisCount);
	ElementsRt2.SetNumZeroed(BoneAxisCount);

	// 加重行列 W
	auto W0 = FBuffer(ElementsW0.GetData(), BoneAxisCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		int32 BoneIndex = BoneIndices[i];
		W0.Ref(i * AXIS_COUNT + 0) = SolverInternals[BoneIndex].X.Weight;
		W0.Ref(i * AXIS_COUNT + 1) = SolverInternals[BoneIndex].Y.Weight;
		W0.Ref(i * AXIS_COUNT + 2) = SolverInternals[BoneIndex].Z.Weight;
	}
}

void FAnimNode_FullbodyIK::GatherDebugData(FNodeDebugData& DebugData)
{
	Super::GatherDebugData(DebugData);
}

void FAnimNode_FullbodyIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Context, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_Eval);

	check(OutBoneTransforms.Num() == 0);

	if (!Setting)
	{
		return;
	}

	auto AnimInstanceObject = Context.AnimInstanceProxy->GetAnimInstanceObject();
	if (!AnimInstanceObject->GetClass()->ImplementsInterface(UAnimInstanceInterface_FullbodyIK::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_FullbodyIK implements UAnimInstanceInterface_FullbodyIK."));
		return;
	}

	auto Mesh = Context.AnimInstanceProxy->GetSkelMeshComponent();
	if (!Mesh)
	{
		return;
	}

	if (Effectors.Effectors.Num() <= 0)
	{
		return;
	}

	CachedAnimInstanceObject = AnimInstanceObject;
	CachedComponentTransform = Context.AnimInstanceProxy->GetComponentTransform();

	// エフェクタ
	TArray<FEffectorInternal> EffectorInternals;
	for (const auto& Effector : Effectors.Effectors)
	{
		if (Effector.EffectorBoneName == NAME_None || Effector.RootBoneName == NAME_None)
		{
			continue;
		}

		FEffectorInternal EffectorInternal;
		EffectorInternal.EffectorType = Effector.EffectorType;
		EffectorInternal.EffectorBoneIndex = Mesh->GetBoneIndex(Effector.EffectorBoneName);
		EffectorInternal.RootBoneIndex = Mesh->GetBoneIndex(Effector.RootBoneName);

		if (EffectorInternal.EffectorBoneIndex == INDEX_NONE || EffectorInternal.RootBoneIndex == INDEX_NONE)
		{
			continue;
		}

		int32 BoneIndex = EffectorInternal.EffectorBoneIndex;

		if (!SolverInternals.Contains(BoneIndex))
		{
			continue;
		}

		EffectorInternal.ParentBoneIndex = SolverInternals[BoneIndex].ParentBoneIndex;
		EffectorInternal.Location = Effector.Location;
		EffectorInternal.Rotation = Effector.Rotation;

		bool bValidation = true;
		while (true)
		{
			int32 ParentBoneIndex = SolverInternals[BoneIndex].ParentBoneIndex;

			if (ParentBoneIndex == INDEX_NONE)
			{
				bValidation = false;
				break;
			}
			else if (ParentBoneIndex == EffectorInternal.RootBoneIndex)
			{
				break;
			}

			BoneIndex = ParentBoneIndex;
		}
		if (!bValidation)
		{
			continue;
		}

		EffectorInternals.Add(EffectorInternal);
	}

	int32 EffectorCount = EffectorInternals.Num();
	if (EffectorCount <= 0)
	{
		return;
	}

	for (int32 BoneIndex : BoneIndices)
	{
		IAnimInstanceInterface_FullbodyIK::Execute_InitializeBoneOffset(AnimInstanceObject, BoneIndex);

		// 初期Transformを保存
		auto CompactPoseBoneIndex = FCompactPoseBoneIndex(BoneIndex);
		auto& SolverInternal = SolverInternals[BoneIndex];
		SolverInternal.LocalTransform = Context.Pose.GetLocalSpaceTransform(CompactPoseBoneIndex);
		SolverInternal.ComponentTransform = Context.Pose.GetComponentSpaceTransform(CompactPoseBoneIndex);
		SolverInternal.InitLocalTransform = SolverInternal.LocalTransform;
		SolverInternal.InitComponentTransform = SolverInternal.ComponentTransform;
	}

	// Transform更新
	SolveSolver(0, FTransform::Identity,
		[&](int32 BoneIndex, FVector& SavedOffsetLocation, FVector& CurrentOffsetLocation)
		{
			CurrentOffsetLocation += SavedOffsetLocation;
		},
		[&](int32 BoneIndex, FRotator& SavedOffsetRotation, FRotator& CurrentOffsetRotation)
		{
			CurrentOffsetRotation += SavedOffsetRotation;
		}
	);

	// 重心の更新
	UpdateCenterOfMass();

	int32 StepLoopCount = 0;
	int32 EffectiveCount = 0;
	while (Setting->StepLoopCountMax > 0 && Setting->EffectiveCountMax > 0 && StepLoopCount < Setting->StepLoopCountMax)
	{
		SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_Loop);

		for (int32 EffectorIndex = 0; EffectorIndex < EffectorCount; ++EffectorIndex)
		{
			const auto& Effector = EffectorInternals[EffectorIndex];
			float EffectorStep[AXIS_COUNT];
			float EtaStep = 0.f;

			FMemory::Memzero(EffectorStep, sizeof(float) * AXIS_COUNT);

			int32 DisplacementCount = 0;
			switch (Effector.EffectorType)
			{
			case EFullbodyIkEffectorType::KeepLocation:
				{
					FVector EndSolverLocation = GetWorldSpaceBoneLocation(Effector.EffectorBoneIndex);
					FVector DeltaLocation = Effector.Location - EndSolverLocation;
					float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}

					DisplacementCount = BoneAxisCount;
				}
				break;
			case EFullbodyIkEffectorType::KeepRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];

					// Transform更新
					SolveSolver(0, FTransform::Identity,
						[&](int32 BoneIndex, FVector& SavedOffsetLocation, FVector& CurrentOffsetLocation)
						{
						},
						[&](int32 BoneIndex, FRotator& SavedOffsetRotation, FRotator& CurrentOffsetRotation)
						{
							if (BoneIndex != Effector.EffectorBoneIndex)
							{
								return;
							}

							FTransform EffectorWorldTransform = FTransform(Effector.Rotation);
							FTransform EffectorComponentTransform = EffectorWorldTransform * CachedComponentTransform.Inverse();
							FTransform EffectorLocalTransform = EffectorComponentTransform * SolverInternals[SolverInternal.ParentBoneIndex].ComponentTransform.Inverse();

							FRotator EffectorLocalRotation = EffectorLocalTransform.Rotator();
							FRotator DeltaLocalRotation = EffectorLocalRotation - CurrentOffsetRotation;

							SavedOffsetRotation += DeltaLocalRotation;
							CurrentOffsetRotation += DeltaLocalRotation;
						}
					);

					DisplacementCount = BoneAxisCount;
				}
				break;
			case EFullbodyIkEffectorType::KeepLocationAndRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];

					// Transform更新
					SolveSolver(0, FTransform::Identity,
						[&](int32 BoneIndex, FVector& SavedOffsetLocation, FVector& CurrentOffsetLocation)
						{
						},
						[&](int32 BoneIndex, FRotator& SavedOffsetRotation, FRotator& CurrentOffsetRotation)
						{
							if (BoneIndex != Effector.EffectorBoneIndex)
							{
								return;
							}

							FTransform EffectorWorldTransform = FTransform(Effector.Rotation);
							FTransform EffectorComponentTransform = EffectorWorldTransform * CachedComponentTransform.Inverse();
							FTransform EffectorLocalTransform = EffectorComponentTransform * SolverInternals[SolverInternal.ParentBoneIndex].ComponentTransform.Inverse();

							FRotator EffectorLocalRotation = EffectorLocalTransform.Rotator();
							FRotator DeltaLocalRotation = EffectorLocalRotation - CurrentOffsetRotation;

							SavedOffsetRotation += DeltaLocalRotation;
							CurrentOffsetRotation += DeltaLocalRotation;
						}
					);

					FVector EndSolverLocation = GetWorldSpaceBoneLocation(Effector.EffectorBoneIndex);
					FVector DeltaLocation = Effector.Location - EndSolverLocation;
					float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}

					DisplacementCount = BoneAxisCount;
				}
				break;
			case EFullbodyIkEffectorType::FollowOriginalLocation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					FTransform InitWorldTransform = SolverInternal.InitComponentTransform * CachedComponentTransform;

					FVector EndSolverLocation = GetWorldSpaceBoneLocation(Effector.EffectorBoneIndex);
					FVector DeltaLocation = InitWorldTransform.GetLocation() + CachedComponentTransform.TransformVector(Effector.Location) - EndSolverLocation;
					float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}

					DisplacementCount = BoneAxisCount;
				}
				break;
			case EFullbodyIkEffectorType::FollowOriginalRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					FTransform InitWorldTransform = SolverInternal.InitComponentTransform * CachedComponentTransform;

					// Transform更新
					SolveSolver(0, FTransform::Identity,
						[&](int32 BoneIndex, FVector& SavedOffsetLocation, FVector& CurrentOffsetLocation)
						{
						},
						[&](int32 BoneIndex, FRotator& SavedOffsetRotation, FRotator& CurrentOffsetRotation)
						{
							if (BoneIndex != Effector.EffectorBoneIndex)
							{
								return;
							}

							FTransform EffectorWorldTransform = FTransform(InitWorldTransform.Rotator());
							FTransform EffectorComponentTransform = EffectorWorldTransform * CachedComponentTransform.Inverse();
							FTransform EffectorLocalTransform = EffectorComponentTransform * SolverInternals[SolverInternal.ParentBoneIndex].ComponentTransform.Inverse();

							FRotator EffectorLocalRotation = EffectorLocalTransform.Rotator();
							FRotator DeltaLocalRotation = EffectorLocalRotation + Effector.Rotation - CurrentOffsetRotation;

							SavedOffsetRotation += DeltaLocalRotation;
							CurrentOffsetRotation += DeltaLocalRotation;
						}
					);

					DisplacementCount = BoneAxisCount;
				}
				break;
			case EFullbodyIkEffectorType::FollowOriginalLocationAndRotation:
				{
					const auto& SolverInternal = SolverInternals[Effector.EffectorBoneIndex];
					FTransform InitWorldTransform = SolverInternal.InitComponentTransform * CachedComponentTransform;

					// Transform更新
					SolveSolver(0, FTransform::Identity,
						[&](int32 BoneIndex, FVector& SavedOffsetLocation, FVector& CurrentOffsetLocation)
						{
						},
						[&](int32 BoneIndex, FRotator& SavedOffsetRotation, FRotator& CurrentOffsetRotation)
						{
							if (BoneIndex != Effector.EffectorBoneIndex)
							{
								return;
							}

							FTransform EffectorWorldTransform = FTransform(InitWorldTransform.Rotator());
							FTransform EffectorComponentTransform = EffectorWorldTransform * CachedComponentTransform.Inverse();
							FTransform EffectorLocalTransform = EffectorComponentTransform * SolverInternals[SolverInternal.ParentBoneIndex].ComponentTransform.Inverse();

							FRotator EffectorLocalRotation = EffectorLocalTransform.Rotator();
							FRotator DeltaLocalRotation = EffectorLocalRotation + Effector.Rotation - CurrentOffsetRotation;

							SavedOffsetRotation += DeltaLocalRotation;
							CurrentOffsetRotation += DeltaLocalRotation;
						}
					);

					FVector EndSolverLocation = GetWorldSpaceBoneLocation(Effector.EffectorBoneIndex);
					FVector DeltaLocation = InitWorldTransform.GetLocation() + CachedComponentTransform.TransformVector(Effector.Location) - EndSolverLocation;
					float DeltaLocationSize = DeltaLocation.Size();

					if (DeltaLocationSize > Setting->ConvergenceDistance)
					{
						float Step = FMath::Min(Setting->StepSize, DeltaLocationSize);
						EtaStep += Step;
						FVector StepV = DeltaLocation / DeltaLocationSize * Step;
						EffectorStep[0] = StepV.X;
						EffectorStep[1] = StepV.Y;
						EffectorStep[2] = StepV.Z;
					}

					DisplacementCount = BoneAxisCount;
				}
				break;
			}
			if (DisplacementCount <= 0)
			{
				continue;
			}

			if (EtaStep <= 0.f)
			{
				continue;
			}

			EtaStep /= Setting->StepSize;

			auto J = FBuffer(ElementsJ.GetData(), DisplacementCount, AXIS_COUNT);
			auto Jt = FBuffer(ElementsJt.GetData(), AXIS_COUNT, DisplacementCount);
			auto JtJ = FBuffer(ElementsJtJ.GetData(), AXIS_COUNT, AXIS_COUNT);
			auto JtJi = FBuffer(ElementsJtJi.GetData(), AXIS_COUNT, AXIS_COUNT);
			auto Jp = FBuffer(ElementsJp.GetData(), AXIS_COUNT, DisplacementCount);
			auto W0 = FBuffer(ElementsW0.GetData(), BoneAxisCount);
			auto Wi = FBuffer(ElementsWi.GetData(), DisplacementCount, DisplacementCount);
			auto JtWi = FBuffer(ElementsJtWi.GetData(), AXIS_COUNT, DisplacementCount);
			auto JtWiJ = FBuffer(ElementsJtWiJ.GetData(), AXIS_COUNT, AXIS_COUNT);
			auto JtWiJi = FBuffer(ElementsJtWiJi.GetData(), AXIS_COUNT, AXIS_COUNT);
			auto JtWiJiJt = FBuffer(ElementsJtWiJiJt.GetData(), AXIS_COUNT, DisplacementCount);
			auto Jwp = FBuffer(ElementsJwp.GetData(), AXIS_COUNT, DisplacementCount);
			auto Rt1 = FBuffer(ElementsRt1.GetData(), BoneAxisCount);
			auto Eta = FBuffer(ElementsEta.GetData(), BoneAxisCount);
			auto EtaJ = FBuffer(ElementsEtaJ.GetData(), AXIS_COUNT);
			auto EtaJJp = FBuffer(ElementsEtaJJp.GetData(), BoneAxisCount);
			auto Rt2 = FBuffer(ElementsRt2.GetData(), BoneAxisCount);

			// ヤコビアン J
			// auto J = FBuffer(DisplacementCount, AXIS_COUNT);
			J.Reset();
			switch (Effector.EffectorType)
			{
			case EFullbodyIkEffectorType::KeepLocation:
			case EFullbodyIkEffectorType::KeepLocationAndRotation:
			case EFullbodyIkEffectorType::FollowOriginalLocation:
			case EFullbodyIkEffectorType::FollowOriginalLocationAndRotation:
				{
					CalcJacobian(Effector, J.Ptr());
				}
				break;
			}

			// J^T
			// auto Jt = FBuffer(AXIS_COUNT, DisplacementCount);
			Jt.Reset();
			MatrixTranspose(Jt.Ptr(), J.Ptr(), DisplacementCount, AXIS_COUNT);

			// J^T * J
			// auto JtJ = FBuffer(AXIS_COUNT, AXIS_COUNT);
			JtJ.Reset();
			MatrixMultiply(
				JtJ.Ptr(),
				Jt.Ptr(), AXIS_COUNT, DisplacementCount,
				J.Ptr(), DisplacementCount, AXIS_COUNT
			);
			for (int32 i = 0; i < AXIS_COUNT; ++i)
			{
				JtJ.Ref(i, i) += Setting->JtJInverseBias;
			}

			// (J^T * J)^-1
			// auto JtJi = FBuffer(AXIS_COUNT, AXIS_COUNT);
			JtJi.Reset();
			float DetJtJ = MatrixInverse3(JtJi.Ptr(), JtJ.Ptr());
			if (DetJtJ == 0)
			{
				continue;
			}

			// ヤコビアン擬似逆行列 (J^T * J)^-1 * J^T
			// auto Jp = FBuffer(AXIS_COUNT, DisplacementCount);
			Jp.Reset();
			MatrixMultiply(
				Jp.Ptr(),
				JtJi.Ptr(), AXIS_COUNT, AXIS_COUNT,
				Jt.Ptr(), AXIS_COUNT, DisplacementCount
			);

			// W^-1
			// auto Wi = FBuffer(DisplacementCount, DisplacementCount);
			Wi.Reset();
			for (int32 i = 0; i < DisplacementCount; ++i)
			{
				Wi.Ref(i, i) = 1.0f / W0.Ref(i % BoneAxisCount);
			}

			// J^T * W^-1
			// auto JtWi = FBuffer(AXIS_COUNT, DisplacementCount);
			JtWi.Reset();
			MatrixMultiply(
				JtWi.Ptr(),
				Jt.Ptr(), AXIS_COUNT, DisplacementCount,
				Wi.Ptr(), DisplacementCount, DisplacementCount
			);

			// J^T * W^-1 * J
			// auto JtWiJ = FBuffer(AXIS_COUNT, AXIS_COUNT);
			JtWiJ.Reset();
			MatrixMultiply(
				JtWiJ.Ptr(),
				JtWi.Ptr(), AXIS_COUNT, DisplacementCount,
				J.Ptr(), DisplacementCount, AXIS_COUNT
			);
			for (int32 i = 0; i < AXIS_COUNT; ++i)
			{
				JtWiJ.Ref(i, i) += Setting->JtJInverseBias;
			}

			// (J^T * W^-1 * J)^-1
			// auto JtWiJi = FBuffer(AXIS_COUNT, AXIS_COUNT);
			JtWiJi.Reset();
			float DetJtWiJ = MatrixInverse3(JtWiJi.Ptr(), JtWiJ.Ptr());
			if (DetJtWiJ == 0)
			{
				continue;
			}

			// (J^T * W^-1 * J)^-1 * J^T
			// auto JtWiJiJt = FBuffer(AXIS_COUNT, DisplacementCount);
			JtWiJiJt.Reset();
			MatrixMultiply(
				JtWiJiJt.Ptr(),
				JtWiJi.Ptr(), AXIS_COUNT, AXIS_COUNT,
				Jt.Ptr(), AXIS_COUNT, DisplacementCount
			);

			// ヤコビアン加重逆行列 (J^T * W^-1 * J)^-1 * J^T * W^-1
			// auto Jwp = FBuffer(AXIS_COUNT, DisplacementCount);
			Jwp.Reset();
			MatrixMultiply(
				Jwp.Ptr(),
				JtWiJiJt.Ptr(), AXIS_COUNT, DisplacementCount,
				Wi.Ptr(), DisplacementCount, DisplacementCount
			);

			// 関節角度ベクトル1 目標エフェクタ変位 * ヤコビアン加重逆行列
			// auto Rt1 = FBuffer(BoneAxisCount);
			Rt1.Reset();
			for (int32 i = 0; i < BoneAxisCount; ++i)
			{
				Rt1.Ref(i) +=
					  EffectorStep[0] * Jwp.Ref(0, i)
					+ EffectorStep[1] * Jwp.Ref(1, i)
					+ EffectorStep[2] * Jwp.Ref(2, i)
					+ EffectorStep[3] * Jwp.Ref(3, i);
			}

			// 冗長変数 Eta
			// auto Eta = FBuffer(BoneAxisCount);
			Eta.Reset();
			if (EtaStep > 0.f)
			{
				for (int32 i = 0; i < BoneCount; ++i)
				{
					SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_CulcEta);

					int32 BoneIndex = BoneIndices[i];
					auto& SolverInternal = SolverInternals[BoneIndex];

					if (SolverInternal.bTranslation)
					{
						FVector CurrentLocation = GetLocalSpaceBoneLocation(BoneIndex);
						FVector InputLocation = SolverInternal.InitLocalTransform.GetLocation();

						auto CalcEta = [&](int32 Axis, float CurrentPosition, float InputPosition, const FFullbodyIKSolverAxis& SolverAxis)
						{
							CurrentPosition += Rt1.Ref(i * AXIS_COUNT + Axis);
							float DeltaPosition = CurrentPosition - InputPosition;
							if (!FMath::IsNearlyZero(DeltaPosition))
							{
								Eta.Ref(i * AXIS_COUNT + Axis) = GetMappedRangeEaseInClamped(
									0, 90,
									0, Setting->EtaSize * SolverAxis.EtaBias * EtaStep,
									1.f, FMath::Abs(DeltaPosition)
								) * (DeltaPosition > 0 ? -1 : 1);
							}
						};

						CalcEta(0, CurrentLocation.X, InputLocation.X, SolverInternal.X);
						CalcEta(1, CurrentLocation.Y, InputLocation.Y, SolverInternal.Y);
						CalcEta(2, CurrentLocation.Z, InputLocation.Z, SolverInternal.Z);
					}
					else
					{
						FRotator CurrentRotation = GetLocalSpaceBoneRotation(BoneIndex).Rotator();
						FRotator InputRotation = SolverInternal.InitLocalTransform.Rotator();

						auto CalcEta = [&](int32 Axis, float CurrentAngle, float InputAngle, const FFullbodyIKSolverAxis& SolverAxis)
						{
							CurrentAngle += FMath::RadiansToDegrees(Rt1.Ref(i * AXIS_COUNT + Axis));
							float DeltaAngle = FRotator::NormalizeAxis(CurrentAngle - InputAngle);
							if (!FMath::IsNearlyZero(DeltaAngle))
							{
								Eta.Ref(i * AXIS_COUNT + Axis) = GetMappedRangeEaseInClamped(
									0, 90,
									0, Setting->EtaSize * SolverAxis.EtaBias * EtaStep,
									1.f, FMath::Abs(DeltaAngle)
								) * (DeltaAngle > 0 ? -1 : 1);
							}
						};

						CalcEta(0, CurrentRotation.Roll, InputRotation.Roll, SolverInternal.X);
						CalcEta(1, CurrentRotation.Pitch, InputRotation.Pitch, SolverInternal.Y);
						CalcEta(2, CurrentRotation.Yaw, InputRotation.Yaw, SolverInternal.Z);
					}
				}
			}

			// Eta * J
			// auto EtaJ = FBuffer(AXIS_COUNT);
			EtaJ.Reset();
			for (int32 i = 0; i < AXIS_COUNT; ++i)
			{
				for (int32 j = 0; j < BoneAxisCount; ++j)
				{
					EtaJ.Ref(i) += Eta.Ref(j) * J.Ref(j, i);
				}
			}

			// Eta * J * J^+
			// auto EtaJJp = FBuffer(BoneAxisCount);
			EtaJJp.Reset();
			for (int32 i = 0; i < BoneAxisCount; ++i)
			{
				for (int32 j = 0; j < AXIS_COUNT; ++j)
				{
					EtaJJp.Ref(i) += EtaJ.Ref(j) * Jp.Ref(j, i);
				}
			}

			// 冗長項 Eta - Eta * J * J^+
			// auto Rt2 = FBuffer(BoneAxisCount);
			Rt2.Reset();
			for (int32 i = 0; i < BoneAxisCount; ++i)
			{
				Rt2.Ref(i) = Eta.Ref(i) - EtaJJp.Ref(i);
			}

			// Transform更新
			SolveSolver(0, FTransform::Identity,
				[&](int32 BoneIndex, FVector& SavedOffsetLocation, FVector& CurrentOffsetLocation)
				{
					int32 BoneIndicesIndex = SolverInternals[BoneIndex].BoneIndicesIndex;

					FVector DeltaLocation = FVector(
						Rt1.Ref(BoneIndicesIndex * AXIS_COUNT + 0) + Rt2.Ref(BoneIndicesIndex * AXIS_COUNT + 0),	// X
						Rt1.Ref(BoneIndicesIndex * AXIS_COUNT + 1) + Rt2.Ref(BoneIndicesIndex * AXIS_COUNT + 1),	// Y
						Rt1.Ref(BoneIndicesIndex * AXIS_COUNT + 2) + Rt2.Ref(BoneIndicesIndex * AXIS_COUNT + 2)		// Z
					);

					SavedOffsetLocation += DeltaLocation;
					CurrentOffsetLocation += DeltaLocation;
				},
				[&](int32 BoneIndex, FRotator& SavedOffsetRotation, FRotator& CurrentOffsetRotation)
				{
					int32 BoneIndicesIndex = SolverInternals[BoneIndex].BoneIndicesIndex;

					FRotator DeltaRotation = FRotator(
						FMath::RadiansToDegrees(Rt1.Ref(BoneIndicesIndex * AXIS_COUNT + 1) + Rt2.Ref(BoneIndicesIndex * AXIS_COUNT + 1)),	// Pitch
						FMath::RadiansToDegrees(Rt1.Ref(BoneIndicesIndex * AXIS_COUNT + 2) + Rt2.Ref(BoneIndicesIndex * AXIS_COUNT + 2)),	// Yaw
						FMath::RadiansToDegrees(Rt1.Ref(BoneIndicesIndex * AXIS_COUNT + 0) + Rt2.Ref(BoneIndicesIndex * AXIS_COUNT + 0))	// Roll
					);

					SavedOffsetRotation += DeltaRotation;
					CurrentOffsetRotation += DeltaRotation;
				}
			);

			++EffectiveCount;

			if (EffectiveCount >= Setting->EffectiveCountMax)
			{
				break;
			}
		}

		// 重心の更新
		UpdateCenterOfMass();

		++StepLoopCount;

		if (EffectiveCount >= Setting->EffectiveCountMax)
		{
			break;
		}
	}

	for (int32 BoneIndex : BoneIndices)
	{
		OutBoneTransforms.Add(FBoneTransform(FCompactPoseBoneIndex(BoneIndex), SolverInternals[BoneIndex].ComponentTransform));
	}

	for (const FName& BoneName : DebugDumpBoneNames)
	{
		int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (SolverInternals.Contains(BoneIndex))
		{
			UE_LOG(LogTemp, Log, TEXT("%s (%d) -----------------"), *BoneName.ToString(), BoneIndex);
			UE_LOG(LogTemp, Log, TEXT("ComponentSpace : %s"), *SolverInternals[BoneIndex].ComponentTransform.ToString());
			UE_LOG(LogTemp, Log, TEXT("    LocalSpace : %s"), *SolverInternals[BoneIndex].LocalTransform.ToString());
			UE_LOG(LogTemp, Log, TEXT("       Rotator : %s"), *SolverInternals[BoneIndex].LocalTransform.Rotator().ToString());
			UE_LOG(LogTemp, Log, TEXT(""));
		}
	}

	if (DebugShowCenterOfMassRadius > 0.f)
	{
		DrawDebugSphere(Mesh->GetWorld(), CenterOfMass, DebugShowCenterOfMassRadius, 16, FColor::Red);
	}

	if (bDebugShowEffectiveCount)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Red, FString::Printf(TEXT("Effective Count : %d"), EffectiveCount));
	}
}

bool FAnimNode_FullbodyIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return true;
}

void FAnimNode_FullbodyIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
}

FFullbodyIKSolver FAnimNode_FullbodyIK::GetSolver(FName BoneName) const
{
	check(Setting);

	for (auto& Solver : Setting->Solvers)
	{
		if (Solver.BoneName == BoneName)
		{
			return Solver;
		}
	}

	// 見つからなければデフォルト
	FFullbodyIKSolver Solver;
	Solver.BoneName = BoneName;

	return Solver;
}

FTransform FAnimNode_FullbodyIK::GetWorldSpaceBoneTransform(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].ComponentTransform;
	Transform *= CachedComponentTransform;
	return Transform;
}

FVector FAnimNode_FullbodyIK::GetWorldSpaceBoneLocation(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].ComponentTransform;
	Transform *= CachedComponentTransform;
	return Transform.GetLocation();
}

FQuat FAnimNode_FullbodyIK::GetWorldSpaceBoneRotation(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].ComponentTransform;
	Transform *= CachedComponentTransform;
	return Transform.GetRotation();
}

FTransform FAnimNode_FullbodyIK::GetLocalSpaceBoneTransform(const int32& BoneIndex) const
{
	return SolverInternals[BoneIndex].LocalTransform;
}

FVector FAnimNode_FullbodyIK::GetLocalSpaceBoneLocation(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].LocalTransform;
	return Transform.GetLocation();
}

FQuat FAnimNode_FullbodyIK::GetLocalSpaceBoneRotation(const int32& BoneIndex) const
{
	FTransform Transform = SolverInternals[BoneIndex].LocalTransform;
	return Transform.GetRotation();
}

void FAnimNode_FullbodyIK::CalcJacobian(const FEffectorInternal& EffectorInternal, float* Jacobian)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_CalcJacobian);

	int32 BoneIndex = EffectorInternal.EffectorBoneIndex;
	const auto EndSolverLocation = GetWorldSpaceBoneLocation(BoneIndex);
	BoneIndex = SolverInternals[BoneIndex].ParentBoneIndex;

	while (true)
	{
		const auto& SolverInternal = SolverInternals[BoneIndex];
		int32 ParentBoneIndex = SolverInternal.ParentBoneIndex;

		FQuat ParentWorldRotation = FQuat::Identity;
		if (ParentBoneIndex != INDEX_NONE)
		{
			ParentWorldRotation = GetWorldSpaceBoneRotation(ParentBoneIndex);
		}

		if (SolverInternal.bTranslation)
		{
			FVector DVec[AXIS_COUNT];
			DVec[0] = FVector(1, 0, 0);
			DVec[1] = FVector(0, 1, 0);
			DVec[2] = FVector(0, 0, 1);

			for (int32 Axis = 0; Axis < AXIS_COUNT; ++Axis)
			{
				// Jacobian[BoneCount * AXIS_COUNT][AXIS_COUNT]
				int32 JacobianIndex1 = SolverInternal.BoneIndicesIndex * AXIS_COUNT;
				int32 JacobianIndex2 = Axis;

				FVector DVec3 = ParentWorldRotation.RotateVector(DVec[Axis]);

				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 0] = DVec3.X;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 1] = DVec3.Y;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 2] = DVec3.Z;
			}
		}
		else
		{
			FVector DeltaLocation = EndSolverLocation - GetWorldSpaceBoneLocation(BoneIndex);
			FVector UnrotateDeltaLocation = GetWorldSpaceBoneRotation(BoneIndex).UnrotateVector(DeltaLocation);
			FRotator BoneRotation = GetLocalSpaceBoneRotation(BoneIndex).Rotator();
			FMatrix DMat[AXIS_COUNT];

			DMat[0] = DiffX(BoneRotation.Roll) * RotY(BoneRotation.Pitch) * RotZ(BoneRotation.Yaw);
			DMat[1] = RotX(BoneRotation.Roll) * DiffY(BoneRotation.Pitch) * RotZ(BoneRotation.Yaw);
			DMat[2] = RotX(BoneRotation.Roll) * RotY(BoneRotation.Pitch) * DiffZ(BoneRotation.Yaw);

			for (int32 Axis = 0; Axis < AXIS_COUNT; ++Axis)
			{
				// Jacobian[BoneCount * AXIS_COUNT][AXIS_COUNT]
				int32 JacobianIndex1 = SolverInternal.BoneIndicesIndex * AXIS_COUNT;
				int32 JacobianIndex2 = Axis;

				FVector4 DVec4 = DMat[Axis].TransformVector(UnrotateDeltaLocation);
				FVector DVec3 = ParentWorldRotation.RotateVector(FVector(DVec4));

				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 0] = DVec3.X;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 1] = DVec3.Y;
				Jacobian[(JacobianIndex1 + JacobianIndex2) * AXIS_COUNT + 2] = DVec3.Z;
			}
		}

		if (BoneIndex == EffectorInternal.RootBoneIndex || ParentBoneIndex == INDEX_NONE)
		{
			break;
		}

		BoneIndex = ParentBoneIndex;
	}
}

void FAnimNode_FullbodyIK::SolveSolver(
	int32 BoneIndex,
	const FTransform& ParentComponentTransform,
	const TFunction<void(int32, FVector&, FVector&)>& LocationOffsetProcess,
	const TFunction<void(int32, FRotator&, FRotator&)>& RotationOffsetProcess)
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_SolveSolver);

	auto& SolverInternal = SolverInternals[BoneIndex];

	if (SolverInternal.bTranslation)
	{
		FVector SavedOffsetLocation = IAnimInstanceInterface_FullbodyIK::Execute_GetBoneLocationOffset(CachedAnimInstanceObject, BoneIndex);
		FVector CurrentOffsetLocation = SolverInternal.LocalTransform.GetLocation();

		LocationOffsetProcess(BoneIndex, SavedOffsetLocation, CurrentOffsetLocation);

		if (SolverInternal.bLimited)
		{
			if (CurrentOffsetLocation.X < SolverInternal.X.LimitMin)
			{
				float Offset = SolverInternal.X.LimitMin - CurrentOffsetLocation.X;
				CurrentOffsetLocation.X += Offset;
				SavedOffsetLocation.X += Offset;
			}
			else if (CurrentOffsetLocation.X > SolverInternal.X.LimitMax)
			{
				float Offset = SolverInternal.X.LimitMax - CurrentOffsetLocation.X;
				CurrentOffsetLocation.X += Offset;
				SavedOffsetLocation.X += Offset;
			}

			if (CurrentOffsetLocation.Y < SolverInternal.Y.LimitMin)
			{
				float Offset = SolverInternal.Y.LimitMin - CurrentOffsetLocation.Y;
				CurrentOffsetLocation.Y += Offset;
				SavedOffsetLocation.Y += Offset;
			}
			else if (CurrentOffsetLocation.Y > SolverInternal.Y.LimitMax)
			{
				float Offset = SolverInternal.Y.LimitMax - CurrentOffsetLocation.Y;
				CurrentOffsetLocation.Y += Offset;
				SavedOffsetLocation.Y += Offset;
			}

			if (CurrentOffsetLocation.Z < SolverInternal.Z.LimitMin)
			{
				float Offset = SolverInternal.Z.LimitMin - CurrentOffsetLocation.Z;
				CurrentOffsetLocation.Z += Offset;
				SavedOffsetLocation.Z += Offset;
			}
			else if (CurrentOffsetLocation.Z > SolverInternal.Z.LimitMax)
			{
				float Offset = SolverInternal.Z.LimitMax - CurrentOffsetLocation.Z;
				CurrentOffsetLocation.Z += Offset;
				SavedOffsetLocation.Z += Offset;
			}
		}

		IAnimInstanceInterface_FullbodyIK::Execute_SetBoneLocationOffset(CachedAnimInstanceObject, BoneIndex, SavedOffsetLocation);
		SolverInternal.LocalTransform.SetLocation(CurrentOffsetLocation);
	}
	else
	{
		FRotator SavedOffsetRotation = IAnimInstanceInterface_FullbodyIK::Execute_GetBoneRotationOffset(CachedAnimInstanceObject, BoneIndex);
		FRotator CurrentOffsetRotation = SolverInternal.LocalTransform.Rotator();

		RotationOffsetProcess(BoneIndex, SavedOffsetRotation, CurrentOffsetRotation);

		if (SolverInternal.bLimited)
		{
			if (CurrentOffsetRotation.Roll < SolverInternal.X.LimitMin)
			{
				float Offset = SolverInternal.X.LimitMin - CurrentOffsetRotation.Roll;
				CurrentOffsetRotation.Roll += Offset;
				SavedOffsetRotation.Roll += Offset;
			}
			else if (CurrentOffsetRotation.Roll > SolverInternal.X.LimitMax)
			{
				float Offset = SolverInternal.X.LimitMax - CurrentOffsetRotation.Roll;
				CurrentOffsetRotation.Roll += Offset;
				SavedOffsetRotation.Roll += Offset;
			}

			if (CurrentOffsetRotation.Pitch < SolverInternal.Y.LimitMin)
			{
				float Offset = SolverInternal.Y.LimitMin - CurrentOffsetRotation.Pitch;
				CurrentOffsetRotation.Pitch += Offset;
				SavedOffsetRotation.Pitch += Offset;
			}
			else if (CurrentOffsetRotation.Pitch > SolverInternal.Y.LimitMax)
			{
				float Offset = SolverInternal.Y.LimitMax - CurrentOffsetRotation.Pitch;
				CurrentOffsetRotation.Pitch += Offset;
				SavedOffsetRotation.Pitch += Offset;
			}

			if (CurrentOffsetRotation.Yaw < SolverInternal.Z.LimitMin)
			{
				float Offset = SolverInternal.Z.LimitMin - CurrentOffsetRotation.Yaw;
				CurrentOffsetRotation.Yaw += Offset;
				SavedOffsetRotation.Yaw += Offset;
			}
			else if (CurrentOffsetRotation.Yaw > SolverInternal.Z.LimitMax)
			{
				float Offset = SolverInternal.Z.LimitMax - CurrentOffsetRotation.Yaw;
				CurrentOffsetRotation.Yaw += Offset;
				SavedOffsetRotation.Yaw += Offset;
			}
		}

		SavedOffsetRotation.Normalize();
		IAnimInstanceInterface_FullbodyIK::Execute_SetBoneRotationOffset(CachedAnimInstanceObject, BoneIndex, SavedOffsetRotation);

		CurrentOffsetRotation.Normalize();
		SolverInternal.LocalTransform.SetRotation(FQuat(CurrentOffsetRotation));
	}

	SolverInternal.ComponentTransform = SolverInternal.LocalTransform * ParentComponentTransform;

	if (SolverTree.Contains(BoneIndex))
	{
		for (auto ChildBoneIndex : SolverTree[BoneIndex])
		{
			SolveSolver(ChildBoneIndex, SolverInternal.ComponentTransform, LocationOffsetProcess, RotationOffsetProcess);
		}
	}
}

void FAnimNode_FullbodyIK::UpdateCenterOfMass()
{
	SCOPE_CYCLE_COUNTER(STAT_FullbodyIK_UpdateCenterOfMass);

	CenterOfMass = FVector::ZeroVector;
	float MassSum = 0.f;

	for (const int32& BoneIndex : BoneIndices)
	{
		const auto& SolverInternal = SolverInternals[BoneIndex];
		const int32& ParentBoneIndex = SolverInternal.ParentBoneIndex;

		if (ParentBoneIndex == INDEX_NONE)
		{
			continue;
		}

		const auto& ParentSolverInternal = SolverInternals[ParentBoneIndex];

		CenterOfMass += (SolverInternal.ComponentTransform.GetLocation() + ParentSolverInternal.ComponentTransform.GetLocation()) * 0.5f * SolverInternal.Mass;
		MassSum += SolverInternal.Mass;
	}

	CenterOfMass /= MassSum;
}

#if WITH_EDITOR
// can't use World Draw functions because this is called from Render of viewport, AFTER ticking component,
// which means LineBatcher already has ticked, so it won't render anymore
// to use World Draw functions, we have to call this from tick of actor
void FAnimNode_FullbodyIK::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp) const
{
	// TODO
}
#endif // WITH_EDITOR
