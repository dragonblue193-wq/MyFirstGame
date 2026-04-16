#include "UMyHealthWidget.h"
#include "Components/ProgressBar.h"

void UMyHealthWidget::UpdateHealthPercent(float CurrentHealth, float MaxHealth)
{
	if (HealthBar)
	{
		// 計算式を修正し、セミコロンで正しく閉じる
		float Percent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;

		Percent = FMath::Clamp(Percent, 0.0f, 1.0f);
		HealthBar->SetPercent(Percent);
	}
}


void UMyHealthWidget::UpdateStaminaPercent(float CurrentStamina, float MaxStamina)
{
	if (StaminaBar)
	{
		float Percent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;

		StaminaBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}