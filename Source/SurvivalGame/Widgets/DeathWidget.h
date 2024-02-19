#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathWidget.generated.h"


UCLASS()
class SURVIVALGAME_API UDeathWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	class ASurvivalCharacter* Killer;

};
