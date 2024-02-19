#pragma once

#include "CoreMinimal.h"
#include "EquippableItem.h"
#include "WeaponItem.generated.h"


UCLASS(Blueprintable)
class SURVIVALGAME_API UWeaponItem : public UEquippableItem
{
	GENERATED_BODY()
public:

	UWeaponItem();

	virtual bool Equip(class ASurvivalCharacter* Character) override;
	virtual bool UnEquip(class ASurvivalCharacter* Character) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<class AWeapon> WeaponClass;
};
