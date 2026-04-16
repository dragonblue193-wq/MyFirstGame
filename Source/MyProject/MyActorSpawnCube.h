// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActorSpawnCube.generated.h"

UCLASS()
class MYPROJECT_API AMyActorSpawnCube : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyActorSpawnCube();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
protected: void SpawnCube();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "MyGame")
	TSubclassOf<AActor>CubeClass;//生成したいCubeクラス（BP）をここに入れる

private: float lastSpawnTime = 0.f;
private: float CurrentTime = 0.f;

protected :
	UFUNCTION()
	void OnCubeHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

};
