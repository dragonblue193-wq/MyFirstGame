// Fill out your copyright notice in the Description page of Project Settings.


#include "MyEnemyHealthWidget.h"
#include "Components/ProgressBar.h"

void UMyEnemyHealthWidget::UpdateHealthPercent(float CurrentHealth, float MaxHealth)
{
	if (HealthBar)
	{
		// 計算式を修正し、セミコロンで正しく閉じる
		float Percent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;

		Percent = FMath::Clamp(Percent, 0.0f, 1.0f);
		HealthBar->SetPercent(Percent);
	}
}

