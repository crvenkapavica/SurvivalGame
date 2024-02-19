#pragma once

#include "CoreMinimal.h"
#include "EquippableItem.h"
#include "GearItem.generated.h"

UCLASS(Blueprintable)
class SURVIVALGAME_API UGearItem : public UEquippableItem
{
	GENERATED_BODY()

public:

	UGearItem();

	virtual bool Equip(class ASurvivalCharacter* Character) override;
	virtual bool UnEquip(class ASurvivalCharacter* Character) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gear")
	class USkeletalMesh* Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gear")
	class UMaterialInstance* MaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gear", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DamageDefenceMultiplier;

};
