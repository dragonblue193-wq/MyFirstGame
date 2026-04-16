// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BPI_Parryable.h" // 1. インクルードする
#include "MyCrystalMagic.generated.h"

class UBoxComponent;
class UNiagaraComponent;

UCLASS()
class MYPROJECT_API AMyCrystalMagic : public AActor, public IBPI_Parryable // 2. ここに書き足す！
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyCrystalMagic();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// ★ 判定用のBox（名前を整理しました）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComp;

	// ★ 見た目のNiagara（新しく追加！）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* NiagaraComp;

	UFUNCTION()
	void OnCrystalOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult // ← 最後を「SweepResult」に統一
	);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
