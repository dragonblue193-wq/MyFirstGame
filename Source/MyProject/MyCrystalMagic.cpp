#include "MyCrystalMagic.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"

AMyCrystalMagic::AMyCrystalMagic()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 判定用の箱
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComp"));
	BoxComp->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	BoxComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = BoxComp;

	// 2. 見た目のエフェクト
	NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	NiagaraComp->SetupAttachment(RootComponent);

	// 3. 重なりイベントのバインド
	BoxComp->OnComponentBeginOverlap.AddDynamic(this, &AMyCrystalMagic::OnCrystalOverlap);

	// ★ 自動消滅（3秒後に消えるように設定）
	InitialLifeSpan = 3.0f;
}

void AMyCrystalMagic::BeginPlay()
{
	Super::BeginPlay();
}

// ★ ここが抜けていました！関数の本体をしっかり記述します
void AMyCrystalMagic::OnCrystalOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	// 自分以外、かつプレイヤーにダメージを与える
	if (OtherActor && OtherActor != GetOwner() && OtherActor->ActorHasTag(FName("Player")))
	{
		UGameplayStatics::ApplyDamage(OtherActor, 25.0f, GetInstigatorController(), this, UDamageType::StaticClass());

		// 5,500本売るための執念：当たったらパッと消える（手応えアップ）
		Destroy();
	}
}

void AMyCrystalMagic::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 前方に移動させる
	AddActorLocalOffset(FVector(1750.f * DeltaTime, 0.f, 0.f));
}