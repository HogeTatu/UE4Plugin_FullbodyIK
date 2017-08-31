#include "AnimGraphNode_FullbodyIK.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_FullbodyIK"

/////////////////////////////////////////////////////
// UAnimGraphNode_FullbodyIK


UAnimGraphNode_FullbodyIK::UAnimGraphNode_FullbodyIK(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_FullbodyIK::GetControllerDescription() const
{
	return LOCTEXT("FullbodyIK", "Fullbody IK");
}

FText UAnimGraphNode_FullbodyIK::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_FullbodyIK_Tooltip", "The Fullbody IK control applies an inverse kinematic (IK) solver to the full body.");
}

FText UAnimGraphNode_FullbodyIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

void UAnimGraphNode_FullbodyIK::CopyNodeDataToPreviewNode(FAnimNode_Base* InPreviewNode)
{
	FAnimNode_FullbodyIK* FullbodyIK = static_cast<FAnimNode_FullbodyIK*>(InPreviewNode);

	// copies Pin values from the internal node to get data which are not compiled yet
	FullbodyIK->Effectors = Node.Effectors;
}

void UAnimGraphNode_FullbodyIK::CopyPinDefaultsToNodeData(UEdGraphPin* InPin)
{
}

void UAnimGraphNode_FullbodyIK::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
}

FEditorModeID UAnimGraphNode_FullbodyIK::GetEditorMode() const
{
	const static FEditorModeID FullbodyIKEditorMode;
	return FullbodyIKEditorMode;
}

void UAnimGraphNode_FullbodyIK::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void UAnimGraphNode_FullbodyIK::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{
	if (bEnableDebugDraw && SkelMeshComp)
	{
		if (FAnimNode_FullbodyIK* ActiveNode = GetActiveInstanceNode<FAnimNode_FullbodyIK>(SkelMeshComp->GetAnimInstance()))
		{
			ActiveNode->ConditionalDebugDraw(PDI, SkelMeshComp);
		}
	}
}

#undef LOCTEXT_NAMESPACE
