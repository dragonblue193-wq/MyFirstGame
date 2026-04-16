// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyMovingCube.generated.h"

UCLASS()
class MYPROJECT_API AMyMovingCube : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyMovingCube();
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* MyCubeMesh;
	UPROPERTY(EditAnywhere, Category = "MySettings")
	float RotationSpeed = 0.2f;
	UPROPERTY(EditAnywhere, Category = "MySettings")
	float FloatAmplitude = 20.0f;
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	UPROPERTY(EditAnywhere, Category = "MySettings")
	float BounceStrength = 1000.0f;
	bool bIsBouncing = false;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable) // Ç±ÇÍÇ≈BPÇ©ÇÁåƒÇ◊ÇÈÇÊÇ§Ç…Ç»ÇÈÅI
		void LaunchMyCube(FVector LaunchDirection);

};
