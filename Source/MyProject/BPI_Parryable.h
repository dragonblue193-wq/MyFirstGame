#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_Parryable.generated.h"

// この UINTERFACE は UE5 の管理用なので触らなくてOK！
UINTERFACE(MinimalAPI)
class UBPI_Parryable : public UInterface
{
	GENERATED_BODY()
};

/**
 * ジャスト回避（パリー）が可能なアクターに持たせるインターフェース
 */
class MYPROJECT_API IBPI_Parryable
{
	GENERATED_BODY()

public:
	// パリー（回避）成功時に相手側から呼ばれる関数
	// BlueprintNativeEvent をつけることで、BP側でもイベントとして実装可能になりますyp！
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Parry")
	void OnParried(AActor* ParriedBy);
};