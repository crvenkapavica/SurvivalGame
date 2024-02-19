#pragma once

#include "CoreMinimal.h"
#include "Item.h"
#include "FoodItem.generated.h"

UCLASS()
class SURVIVALGAME_API UFoodItem : public UItem
{
	GENERATED_BODY()
	
public:

	UFoodItem();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Healing")
	float HealAmount;

	virtual void Use(class ASurvivalCharacter* Character) override;
};
