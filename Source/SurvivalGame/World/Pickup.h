#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pickup.generated.h"

UCLASS(ClassGroup = (Items), Blueprintable, Abstract)
class SURVIVALGAME_API APickup : public AActor
{
	GENERATED_BODY()
	
public:	
	APickup();

	void InitializePickup(const TSubclassOf<class UItem> ItemClass, const int32 Quantity);

	UFUNCTION(BlueprintImplementableEvent)
	void AlignWithGround();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	class UItem* ItemTemplate;


protected:
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, ReplicatedUsing = OnRep_Item)
	class UItem* Item;

	UFUNCTION()
	void OnRep_Item();

	UFUNCTION()
	void OnItemModified();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 

	UFUNCTION()
	void OnTakePickup(class ASurvivalCharacter* Taker);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PickupMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	class UInteractionComponent* InteractionComponent;
};
