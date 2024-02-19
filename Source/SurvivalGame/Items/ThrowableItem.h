#pragma once

#include "CoreMinimal.h"
#include "EquippableItem.h"
#include "ThrowableItem.generated.h"


UCLASS(Blueprintable)
class SURVIVALGAME_API UThrowableItem : public UEquippableItem
{
	GENERATED_BODY()
	
public:

	UThrowableItem();

	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	class UAnimMontage* ThrowableTossAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<class AThrowableWeapon> ThrowableClass;

};
