#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "CommonAnimTypes.h"
#include "FullbodyIKSetting.h"
#include "AnimNode_FullbodyIK.generated.h"

class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EFullbodyIkEffectorType : uint8
{
	KeepLocation,
	KeepRotation,
	KeepLocationAndRotation,
	FollowOriginalLocation,
	FollowOriginalRotation,
	FollowOriginalLocationAndRotation,
};

USTRUCT(BlueprintType)
struct FULLBODYIK_API FAnimNode_FullbodyIkEffector
{
	GENERATED_BODY()

public:
	FAnimNode_FullbodyIkEffector()
		: EffectorType(EFullbodyIkEffectorType::KeepLocation)
		, EffectorBoneName(NAME_None)
		, RootBoneName(NAME_None)
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector)
	EFullbodyIkEffectorType EffectorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector)
	FName EffectorBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector)
	FName RootBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector)
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector)
	FRotator Rotation;
};

USTRUCT(BlueprintType)
struct FULLBODYIK_API FAnimNode_FullbodyIkEffectors
{
	GENERATED_BODY()

public:
	FAnimNode_FullbodyIkEffectors()
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector)
	TArray<FAnimNode_FullbodyIkEffector> Effectors;
};

USTRUCT(BlueprintInternalUseOnly)
struct FULLBODYIK_API FAnimNode_FullbodyIK : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=IK)
	TArray<FName> IkEndBoneNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=IK)
	UFullbodyIKSetting* Setting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=IK)
	int32 EffectorCountMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=EndEffector, meta=(PinShownByDefault))
	FAnimNode_FullbodyIkEffectors Effectors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=IK)
	FVector CenterOfMass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=IK)
	TArray<FName> DebugDumpBoneNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=IK)
	float DebugShowCenterOfMassRadius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=IK)
	bool bDebugShowEffectiveCount;

	FAnimNode_FullbodyIK();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
#if WITH_EDITOR
	void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp) const;
#endif // WITH_EDITOR
	// End of FAnimNode_SkeletalControlBase interface

private:
	struct FBuffer
	{
	public:
		FBuffer()
			: Elements(nullptr)
			, SizeX(0)
			, SizeY(0)
		{
		}

		FBuffer(float* InElements, int32 InSizeX)
			: Elements(InElements)
			, SizeX(InSizeX)
			, SizeY(1)
		{
			Elements = new (InElements) float(sizeof(float) * SizeX * SizeY);
		}

		FBuffer(float* InElements, int32 InSizeY, int32 InSizeX)
			: Elements(InElements)
			, SizeX(InSizeX)
			, SizeY(InSizeY)
		{
			Elements = new (InElements) float(sizeof(float) * SizeX * SizeY);
		}

		void Reset()
		{
			FMemory::Memzero(Elements, sizeof(float) * SizeX * SizeY);
		}

		float& Ref(int32 X)
		{
			return Elements[X];
		}

		float& Ref(int32 Y, int32 X)
		{
			return Elements[Y * SizeX + X];
		}

		float* Ptr()
		{
			return Elements;
		}

	private:
		float* Elements;
		int32 SizeX;
		int32 SizeY;
	};

	struct FSolverInternal
	{
	public:
		FSolverInternal()
			: BoneIndex(INDEX_NONE)
			, ParentBoneIndex(INDEX_NONE)
			, BoneIndicesIndex(INDEX_NONE)
			, LocalTransform(FTransform::Identity)
			, ComponentTransform(FTransform::Identity)
			, InitLocalTransform(FTransform::Identity)
			, InitComponentTransform(FTransform::Identity)
			, bTranslation(false)
			, bLimited(false)
			, Mass(1.f)
		{
		}

		int32 BoneIndex;
		int32 ParentBoneIndex;
		int32 BoneIndicesIndex;
		FTransform LocalTransform;
		FTransform ComponentTransform;
		FTransform InitLocalTransform;
		FTransform InitComponentTransform;
		bool bTranslation;
		bool bLimited;
		float Mass;
		FFullbodyIKSolverAxis X;
		FFullbodyIKSolverAxis Y;
		FFullbodyIKSolverAxis Z;
	};

	struct FEffectorInternal
	{
	public:
		FEffectorInternal()
			: EffectorType(EFullbodyIkEffectorType::KeepLocation)
			, EffectorBoneIndex(INDEX_NONE)
			, RootBoneIndex(INDEX_NONE)
			, ParentBoneIndex(INDEX_NONE)
			, Location(FVector::ZeroVector)
			, Rotation(FRotator::ZeroRotator)
		{
		}

		EFullbodyIkEffectorType EffectorType;
		int32 EffectorBoneIndex;
		int32 RootBoneIndex;
		int32 ParentBoneIndex;
		FVector Location;
		FRotator Rotation;
	};

	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	FFullbodyIKSolver GetSolver(FName BoneName) const;

	FTransform GetWorldSpaceBoneTransform(const int32& BoneIndex) const;
	FVector GetWorldSpaceBoneLocation(const int32& BoneIndex) const;
	FQuat GetWorldSpaceBoneRotation(const int32& BoneIndex) const;
	FTransform GetLocalSpaceBoneTransform(const int32& BoneIndex) const;
	FVector GetLocalSpaceBoneLocation(const int32& BoneIndex) const;
	FQuat GetLocalSpaceBoneRotation(const int32& BoneIndex) const;

	void CalcJacobian(const FEffectorInternal& EffectorInternal, float* Jacobian);

	void SolveSolver(
		int32 BoneIndex,
		const FTransform& ParentComponentTransform,
		const TFunction<void(int32, FVector&, FVector&)>& LocationOffsetProcess,
		const TFunction<void(int32, FRotator&, FRotator&)>& RotationOffsetProcess);

	void UpdateCenterOfMass();

	TArray<int32> BoneIndices;
	int32 BoneCount;
	int32 BoneAxisCount;

	TMap<int32, FSolverInternal> SolverInternals;
	TMap<int32, TArray<int32>> SolverTree;

	UObject* CachedAnimInstanceObject;
	FTransform CachedComponentTransform;

	TArray<float> ElementsJ;
	TArray<float> ElementsJt;
	TArray<float> ElementsJtJ;
	TArray<float> ElementsJtJi;
	TArray<float> ElementsJp;
	TArray<float> ElementsW0;
	TArray<float> ElementsWi;
	TArray<float> ElementsJtWi;
	TArray<float> ElementsJtWiJ;
	TArray<float> ElementsJtWiJi;
	TArray<float> ElementsJtWiJiJt;
	TArray<float> ElementsJwp;
	TArray<float> ElementsRt1;
	TArray<float> ElementsEta;
	TArray<float> ElementsEtaJ;
	TArray<float> ElementsEtaJJp;
	TArray<float> ElementsRt2;
};
