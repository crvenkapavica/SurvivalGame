#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThrowableWeapon.generated.h"

UCLASS()
class SURVIVALGAME_API AThrowableWeapon : public AActor
{
	GENERATED_BODY()
	
public:
	AThrowableWeapon();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Components")
	class UStaticMeshComponent* ThrowableMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	class UProjectileMovementComponent* ThrowableMovement;
};
