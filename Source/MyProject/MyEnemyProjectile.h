// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "CombatInterface.h"
#include "MyEnemyProjectile.generated.h"


class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;


UCLASS()
class MYPROJECT_API AMyEnemyProjectile : public AActor, public ICombatInterface

{
	GENERATED_BODY()
	
public:	

	virtual FVector GetFocusLocation() const override;

	// Sets default values for this actor's properties
	AMyEnemyProjectile();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UNiagaraComponent* NiagaraComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }




protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
