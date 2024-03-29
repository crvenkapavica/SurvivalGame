#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SurvivalGameStateBase.generated.h"

UCLASS()
class SURVIVALGAME_API ASurvivalGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASurvivalGameStateBase();

	UPROPERTY(EditDefaultsOnly, Category = "Game Rules")
	float RespawnTime;

};
