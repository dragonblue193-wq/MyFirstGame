// Fill out your copyright notice in the Description page of Project Settings.


#include "MyActorSpawnCube.h"
#include "Engine/World.h" 

// Sets default values
AMyActorSpawnCube::AMyActorSpawnCube()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMyActorSpawnCube::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyActorSpawnCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - lastSpawnTime > 3.f)
	{
		SpawnCube();
	}

}
void AMyActorSpawnCube::SpawnCube()
{
	if (CubeClass)
	{
		FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 200.0f;
		FRotator SpawnRotation = GetActorRotation();

		AActor* SpawnedCube = GetWorld()->SpawnActor<AActor>(CubeClass, SpawnLocation, SpawnRotation);
		if (SpawnedCube)
		{
			APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();

			if (PlayerPawn)
			{
				FVector Direction = PlayerPawn->GetActorLocation() - SpawnedCube -> GetActorLocation();
				Direction.Normalize();

				UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(SpawnedCube->GetRootComponent());
				if (mesh)
				{
					UE_LOG(LogTemp, Warning, TEXT("Force Applied to Mesh!")); // これが出るかチェック！
					mesh->SetSimulatePhysics(true);
					mesh->AddImpulse(Direction * 1000.0f * mesh->GetMass());

					mesh->SetNotifyRigidBodyCollision(true);
				}
				else {
					UE_LOG(LogTemp, Error, TEXT("Mesh NOT found! Root is not a StaticMesh!"));
				}

				SpawnedCube->OnActorHit.AddDynamic(this, &AMyActorSpawnCube::OnCubeHit);

			}
			UE_LOG(LogTemp, Warning, TEXT("Cube Spawned! Success!"));
			lastSpawnTime = CurrentTime;
		}
	}
}
void AMyActorSpawnCube::OnCubeHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	// 当たった相手がプレイヤー（Pawn）かどうかを確認
	if (OtherActor && OtherActor != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("石が衝突！粉砕しますyp！"));

		// 当たった石（自分自身）を消滅させる
		SelfActor->Destroy();
	}
}

