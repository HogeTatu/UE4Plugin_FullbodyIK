#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AnimInstanceInterface_FullbodyIK.h"
#include "AnimInstanceFullbodyIK.generated.h"

UCLASS(transient, Blueprintable, hideCategories=AnimInstance, BlueprintType, meta=(BlueprintThreadSafe))
class FULLBODYIK_API UAnimInstanceFullbodyIK : public UAnimInstance, public IAnimInstanceInterface_FullbodyIK
{
	GENERATED_BODY()

public:
	UAnimInstanceFullbodyIK(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeBoneOffset_Implementation(int32 BoneIndex) override;
	virtual void SetBoneLocationOffset_Implementation(int32 BoneIndex, const FVector& Location) override;
	virtual FVector GetBoneLocationOffset_Implementation(int32 BoneIndex) const override;
	virtual void SetBoneRotationOffset_Implementation(int32 BoneIndex, const FRotator& Rotation) override;
	virtual FRotator GetBoneRotationOffset_Implementation(int32 BoneIndex) const override;

private:
	TMap<int32, FVector> OffsetLocations;
	TMap<int32, FRotator> OffsetRotations;
};
