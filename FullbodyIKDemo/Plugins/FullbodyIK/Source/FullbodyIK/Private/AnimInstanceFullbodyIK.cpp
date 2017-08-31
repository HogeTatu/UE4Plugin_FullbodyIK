#include "AnimInstanceFullbodyIK.h"

UAnimInstanceFullbodyIK::UAnimInstanceFullbodyIK(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimInstanceFullbodyIK::InitializeBoneOffset_Implementation(int32 BoneIndex)
{
	if (!OffsetLocations.Contains(BoneIndex))
	{
		OffsetLocations.Add(BoneIndex, FVector::ZeroVector);
	}
	if (!OffsetRotations.Contains(BoneIndex))
	{
		OffsetRotations.Add(BoneIndex, FRotator::ZeroRotator);
	}
}

void UAnimInstanceFullbodyIK::SetBoneLocationOffset_Implementation(int32 BoneIndex, const FVector& Location)
{
	OffsetLocations[BoneIndex] = Location;
}

FVector UAnimInstanceFullbodyIK::GetBoneLocationOffset_Implementation(int32 BoneIndex) const
{
	return OffsetLocations[BoneIndex];
}

void UAnimInstanceFullbodyIK::SetBoneRotationOffset_Implementation(int32 BoneIndex, const FRotator& Rotation)
{
	OffsetRotations[BoneIndex] = Rotation;
}

FRotator UAnimInstanceFullbodyIK::GetBoneRotationOffset_Implementation(int32 BoneIndex) const
{
	return OffsetRotations[BoneIndex];
}
