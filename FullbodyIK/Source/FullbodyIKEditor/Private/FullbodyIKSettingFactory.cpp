#include "FullbodyIKSettingFactory.h"
#include "FullbodyIKSetting.h"
#include "AssetTypeCategories.h"

UFullbodyIKSettingFactory::UFullbodyIKSettingFactory()
{
	SupportedClass = UFullbodyIKSetting::StaticClass();
	bCreateNew = true;
}

UObject* UFullbodyIKSettingFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return CastChecked<UFullbodyIKSetting>(StaticConstructObject_Internal(InClass, InParent, InName, Flags));
}

uint32 UFullbodyIKSettingFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Animation;
}
