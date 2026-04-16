// Fill out your copyright notice in the Description page of Project Settings.


#include "MyMovingCube.h"
#include "Kismet/KismetMathLibrary.h" // FMathを使うために必要になる場合があります
#include "Components/StaticMeshComponent.h"

// Sets default values
AMyMovingCube::AMyMovingCube()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	MyCubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MyVisualMesh"));
	RootComponent = MyCubeMesh;
	MyCubeMesh->SetSimulatePhysics(true);
	MyCubeMesh->SetNotifyRigidBodyCollision(true);
	MyCubeMesh->OnComponentHit.AddDynamic(this, &AMyMovingCube::OnHit);

}

// Called when the game starts or when spawned
void AMyMovingCube::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyMovingCube::Tick(float DeltaTime)
{
	if (!bIsBouncing)
	{
		Super::Tick(DeltaTime);
		FRotator RotationAmount(0.f, RotationSpeed, 0.f);
		MyCubeMesh->AddLocalRotation(RotationAmount);

		//現在のゲーム開始からの時間を計算
		float RunningTime = GetGameTimeSinceCreation();

		//Sinを使って次のフレームまでの高さの差分を計算
		float DeltaHeight = (FMath::Sin(RunningTime + DeltaTime) - FMath::Sin(RunningTime));

		//Z軸の高さを計算
		FVector MovementAmount(0.f, 0.f, DeltaHeight * FloatAmplitude);
		AddActorLocalOffset(MovementAmount);
	}
	
}
void AMyMovingCube::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor != this)
		bIsBouncing = true;
	{
		FVector ImpulseDirection = Hit.ImpactNormal * -1.0f;

		ImpulseDirection.Z += 0.5f;

		MyCubeMesh->AddImpulse(ImpulseDirection * BounceStrength * MyCubeMesh->GetMass());
	}
}
void AMyMovingCube::LaunchMyCube(FVector LaunchDirection)
{
	bIsBouncing = true;
	UE_LOG(LogTemp, Warning, TEXT("!!! Yobareta !!! LaunchMyCube has been called!"));
	MyCubeMesh->AddImpulse(LaunchDirection * BounceStrength * MyCubeMesh->GetMass());
}

