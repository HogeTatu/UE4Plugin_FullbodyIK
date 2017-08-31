#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "FullbodyIKSettingFactory.generated.h"

UCLASS()
class UFullbodyIKSettingFactory : public UFactory
{
	GENERATED_BODY()

public:
	UFullbodyIKSettingFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const;
};
