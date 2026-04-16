// Fill out your copyright notice in the Description page of Project Settings.

#include "MyEnemyProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h" // 追加

// Sets default values
AMyEnemyProjectile::AMyEnemyProjectile()
{
    // 1. 衝突コンポーネントの設定
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(15.0f);

    // 衝突プロファイルとチャンネルの設定
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // ★最重要：これが false だと OnComponentHit が呼ばれません（Simulation Generates Hit Events）
    CollisionComp->SetNotifyRigidBodyCollision(true);

    // 各チャンネルへの応答設定（Player(Pawn)を確実にブロックする）
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

    RootComponent = CollisionComp;

    // 2. ビジュアル（Niagara）の設定
    NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
    NiagaraComp->SetupAttachment(RootComponent);

    // 3. 移動コンポーネントの設定
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 1500.0f;
    ProjectileMovement->MaxSpeed = 1500.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.f;

    if (CollisionComp)
    {
        CollisionComp->BodyInstance.bUseCCD = true;
    }

    InitialLifeSpan = 5.0f;

    // ヒットイベントのバインド
    if (CollisionComp)
    {
        CollisionComp->OnComponentHit.AddDynamic(this, &AMyEnemyProjectile::OnHit);
    }

    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMyEnemyProjectile::BeginPlay()
{
    Super::BeginPlay();
    // 念のため開始ログ
    UE_LOG(LogTemp, Log, TEXT("Projectile Spawned: %s"), *GetName());
}

// Called every frame
void AMyEnemyProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AMyEnemyProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // デバッグメッセージ（画面左上に表示）
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("HIT DETECTED!"));
    }

    // 当たった相手が自分自身やオーナー（撃った本人）でないかチェック
    if (OtherActor && OtherActor != this && OtherActor != GetOwner())
    {
        // ダメージを与える
        OtherActor->TakeDamage(20.0f, FDamageEvent(), GetInstigatorController(), this);

        // ログ出力
        UE_LOG(LogTemp, Warning, TEXT("Hit Actor: %s"), *OtherActor->GetName());

        // 弾を消す
        Destroy();
    }
    else if (!OtherActor)
    {
        UE_LOG(LogTemp, Error, TEXT("OtherActor is NULL during OnHit!"));
    }
}

FVector AMyEnemyProjectile::GetFocusLocation() const
{
    // パリー演出などでカメラがこの弾を追うための座標
    return GetActorLocation();
}