// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyEnemyHealthWidget.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECT_API UMyEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateHealthPercent(float CurrentHealth, float MaxHealth);
	
	// ★ C++から呼ぶための「合図」を作る
	// BlueprintImplementableEvent を付けると、WBP側でノードとして出現します
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void PlayShakeAnimation(float Intensity);
	
};
