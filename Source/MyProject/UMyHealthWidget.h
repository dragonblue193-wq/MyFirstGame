// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UMyHealthWidget.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECT_API UMyHealthWidget : public UUserWidget
{
	GENERATED_BODY()
	

public :
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

	UFUNCTION (BlueprintCallable, Category = "UI")
	void UpdateHealthPercent(float CurrentHealth, float MaxHealth);
   //------スタミナ用----------------------------------------

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* StaminaBar;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateStaminaPercent(float CurrentStamina, float MaxStamina);
   
};
