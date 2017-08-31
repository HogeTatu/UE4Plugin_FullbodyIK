#pragma once

#include "CoreMinimal.h"
#include "FullbodyIKSetting.generated.h"

USTRUCT(BlueprintType)
struct FULLBODYIK_API FFullbodyIKSolverAxis
{
	GENERATED_BODY()

public:
	FFullbodyIKSolverAxis()
		: Weight(1.f)
		, LimitMin(-180.f)
		, LimitMax(180.f)
		, EtaBias(1.f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float Weight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float LimitMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float LimitMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float EtaBias;
};

USTRUCT(BlueprintType)
struct FULLBODYIK_API FFullbodyIKSolver
{
	GENERATED_BODY()

public:
	FFullbodyIKSolver()
		: BoneName(NAME_None)
		, bTranslation(false)
		, bLimited(false)
		, Mass(1.f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	bool bTranslation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	bool bLimited;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float Mass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	FFullbodyIKSolverAxis X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	FFullbodyIKSolverAxis Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	FFullbodyIKSolverAxis Z;
};

UCLASS()
class FULLBODYIK_API UFullbodyIKSetting : public UObject
{
	GENERATED_BODY()

public:
	UFullbodyIKSetting()
		: ConvergenceDistance(0.1f)
		, StepSize(10.f)
		, StepLoopCountMax(10)
		, EffectiveCountMax(10)
		, EtaSize(0.f)
		, JtJInverseBias(0.f)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	TArray<FFullbodyIKSolver> Solvers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float ConvergenceDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float StepSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	int32 StepLoopCountMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	int32 EffectiveCountMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float EtaSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IK)
	float JtJInverseBias;
};
