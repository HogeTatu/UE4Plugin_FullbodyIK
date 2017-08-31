#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AnimInstanceInterface_FullbodyIK.generated.h"

UINTERFACE()
class FULLBODYIK_API UAnimInstanceInterface_FullbodyIK : public UInterface
{
	GENERATED_BODY()
};

class FULLBODYIK_API IAnimInstanceInterface_FullbodyIK
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category="IK")
	void InitializeBoneOffset(int32 BoneIndex);

	UFUNCTION(BlueprintNativeEvent, Category="IK")
	void SetBoneLocationOffset(int32 BoneIndex, const FVector& Location);

	UFUNCTION(BlueprintNativeEvent, Category="IK")
	FVector GetBoneLocationOffset(int32 BoneIndex) const;

	UFUNCTION(BlueprintNativeEvent, Category="IK")
	void SetBoneRotationOffset(int32 BoneIndex, const FRotator& Rotation);

	UFUNCTION(BlueprintNativeEvent, Category="IK")
	FRotator GetBoneRotationOffset(int32 BoneIndex) const;
};
