#include "SurvivalHUD.h"
#include "Widgets/InventoryWidget.h"
#include "Widgets/NotificationWidget.h"
#include "Widgets/MapWidget.h"
#include "Widgets/LootWidget.h"
#include "Widgets/PauseWidget.h"
#include "Widgets/GameplayWidget.h"
#include "Widgets/DeathWidget.h"
#include "InventoryWidget.h"

ASurvivalHUD::ASurvivalHUD()
{

}

void ASurvivalHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!InventoryWidget && InventoryWidgetClass && PlayerOwner)
	{
		InventoryWidget = CreateWidget<UInventoryWidget>(PlayerOwner, InventoryWidgetClass);
	}
}

void ASurvivalHUD::OpenInventoryWidget()
{
	if (!InventoryWidget && InventoryWidgetClass && PlayerOwner)
	{
		InventoryWidget = CreateWidget<UInventoryWidget>(PlayerOwner, InventoryWidgetClass);
	}

	if (InventoryWidget && !InventoryWidget->IsInViewport())
	{
		InventoryWidget->AddToViewport();

		FInputModeUIOnly UIInput;
		UIInput.SetWidgetToFocus(InventoryWidget->TakeWidget());

		PlayerOwner->bShowMouseCursor = true;
		PlayerOwner->SetInputMode(UIInput);
	}
}

void ASurvivalHUD::OpenPauseWidget()
{
	if (!PauseWidget && PauseWidgetClass && PlayerOwner)
	{
		PauseWidget = CreateWidget<UPauseWidget>(PlayerOwner, PauseWidgetClass);
	}

	if (PauseWidget && !PauseWidget->IsInViewport())
	{
		PauseWidget->AddToViewport(2);

		FInputModeUIOnly UIInput;
		UIInput.SetWidgetToFocus(PauseWidget->TakeWidget());

		PlayerOwner->bShowMouseCursor = true;
		PlayerOwner->SetInputMode(UIInput);
	}
}

void ASurvivalHUD::ClosePauseWidget()
{
	if (PauseWidget && PlayerOwner)
	{
		PauseWidget->RemoveFromViewport();
		PlayerOwner->bShowMouseCursor = false;
		PlayerOwner->SetInputMode(FInputModeGameOnly());
	}
}

void ASurvivalHUD::OpenLootWidget()
{
	if (!LootWidget && LootWidgetClass && PlayerOwner)
	{
		LootWidget = CreateWidget<ULootWidget>(PlayerOwner, LootWidgetClass);
	}

	if (LootWidget)
	{
		LootWidget->AddToViewport();

		FInputModeUIOnly UIInput;
		UIInput.SetWidgetToFocus(LootWidget->TakeWidget());

		PlayerOwner->bShowMouseCursor = true;
		PlayerOwner->SetInputMode(UIInput);
	}
}

void ASurvivalHUD::CloseLootWidget()
{
	if (LootWidget && LootWidget->IsInViewport())
	{
		LootWidget->RemoveFromViewport();

		PlayerOwner->bShowMouseCursor = true;
		PlayerOwner->SetInputMode(FInputModeGameOnly());
	}
}

void ASurvivalHUD::CreateGameplayWidget()
{
	if (DeathWidget)
	{
		DeathWidget->RemoveFromViewport();
	}

	if (!NotificationWidget)
	{
		CreateNotificationWidget();
	}

	if (!GameplayWidget && GameplayWidgetClass && PlayerOwner)
	{
		GameplayWidget = CreateWidget<UGameplayWidget>(PlayerOwner, GameplayWidgetClass);

		PlayerOwner->bShowMouseCursor = false;
		PlayerOwner->SetInputMode(FInputModeGameOnly());
	}

	if (GameplayWidget && !GameplayWidget->IsInViewport())
	{
		GameplayWidget->AddToViewport();
	}
}

void ASurvivalHUD::CreateNotificationWidget()
{
	if (!NotificationWidget && NotificationWidgetClass && PlayerOwner)
	{
		NotificationWidget = CreateWidget<UNotificationWidget>(PlayerOwner, NotificationWidgetClass);
	}

	if (NotificationWidget)
	{
		NotificationWidget->AddToViewport(1);
	}
}

void ASurvivalHUD::ShowNotification(const FText& NotificationText)
{
	if (NotificationWidget)
	{
		NotificationWidget->ShowNotification(NotificationText);
	}
}

void ASurvivalHUD::ShowDeathWidget(class ASurvivalCharacter* Killer)
{
	if (GameplayWidget)
	{
		GameplayWidget->RemoveFromViewport();
	}

	if (!DeathWidget && DeathWidgetClass && PlayerOwner)
	{
		DeathWidget = CreateWidget<UDeathWidget>(PlayerOwner, DeathWidgetClass);
	}

	if (DeathWidget)
	{
		DeathWidget->Killer = Killer;
		DeathWidget->AddToViewport();
	}
}

void ASurvivalHUD::OpenMap()
{
	if (!MapWidget && MapWidgetClass && PlayerOwner)
	{
		MapWidget = CreateWidget<UMapWidget>(PlayerOwner, MapWidgetClass);
	}

	if (MapWidget)
	{
		MapWidget->AddToViewport(1);
	}
}

void ASurvivalHUD::CloseMap()
{
	if (MapWidget)
	{
		MapWidget->RemoveFromViewport();
	}
}

void ASurvivalHUD::ShowHitmarker()
{
	if (GameplayWidget)
	{
		GameplayWidget->ShowHitmarker();
	}
}

bool ASurvivalHUD::IsInventoryOpen() const
{
	return InventoryWidget ? InventoryWidget->IsInViewport() : false;
}
