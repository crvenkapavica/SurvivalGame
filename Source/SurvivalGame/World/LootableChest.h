#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootableChest.generated.h"

UCLASS(ClassGroup = (Items), Blueprintable, Abstract)
class SURVIVALGAME_API ALootableChest : public AActor
{
	GENERATED_BODY()
	
public:	
	ALootableChest();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* LootContainerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class UInteractionComponent* LootInteraction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class UInventoryComponent* Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class UDataTable* LootTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	FIntPoint LootRolls;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnInteract(class ASurvivalCharacter* Character);

};
