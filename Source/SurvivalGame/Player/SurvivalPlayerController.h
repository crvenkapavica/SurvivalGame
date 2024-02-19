#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalPlayerController.generated.h"

class UQuest;
class UQuestState;

UCLASS()
class SURVIVALGAME_API ASurvivalPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:


	ASurvivalPlayerController();
	
	UFUNCTION(Client, Reliable, BlueprintCallable)
	void ClientShowNotification(const FText& Message);

protected:

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

public:
	void Died(class ASurvivalCharacter* Killer);

	UFUNCTION(Client, Unreliable)
	void ClientShotHitConfirmed();

	UFUNCTION(BlueprintCallable)
	void Respawn();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRespawn();

	UFUNCTION(BlueprintPure)
	bool IsInventoryOpen() const;

protected:
	void OpenInventory();

	void OpenMap();
	void CloseMap();

	void PauseGame();

	UFUNCTION(BlueprintCallable)
	void ResumeGame();

public:
	void ApplyRecoil(const FVector2D& RecoilAmount, const float RecoilSpeed, const float RecoilResetSpeed);

	UPROPERTY(VisibleAnywhere, Category = "Recoil")
	FVector2D RecoilBumpAmount;

	UPROPERTY(VisibleAnywhere, Category = "Recoil")
	FVector2D RecoilResetAmount;

	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float CurrentRecoilSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float CurrentRecoilResetSpeed;

	UPROPERTY(VisibleAnywhere, Category = "Recoil")
	float LastRecoilTime;

	void Turn(float Rate);
	void LookUp(float Rate);

	void StartReload();

};
