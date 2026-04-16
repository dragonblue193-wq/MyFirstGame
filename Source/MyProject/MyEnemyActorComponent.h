#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyEnemyHealthWidget.h" // 先ほど作ったUIのヘッダーをインクルード
#include "CombatInterface.h"
#include "NiagaraComponent.h"
#include "Camera/CameraComponent.h"
#include "MyEnemyActorComponent.generated.h"

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Chasing UMETA(DisplayName = "Chasing"),
	Dashing UMETA(DisplayName = "Dashing"),
	Attacking UMETA(DisplayName = "Attacking"),
	Recovery UMETA(DisplayName = "Recovery"),
	Stunned UMETA(DisplayName = "Stunned"),
	ChronosJitter UMETA(DisplayName = "ChronosJitter"),
	Dead UMETA(DisplayName = "Dead"),
};

enum class EAttackType : uint8
{
	DashMelee,    //突進して切る
	BackStepAttack,  // その場でコンボ
	BehindAttack,  // 頭上をジャンプして背後から攻撃
	SequenceAttack,//連続攻撃
	CrystalMagic,//クリスタル生成
	AreaBlast,    //周囲に衝撃波
	JumpAttack//playerめがけてジャンプして攻撃
};

UENUM(BlueprintType)
	enum class EDirectionalSector : uint8
{
	Up,
	Down,
	Left,
	Right,
	None
};

class UAnimMontage;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FComponentDeadSignature);

class AMyCrystalMagic;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UMyEnemyActorComponent : public UActorComponent, public ICombatInterface
{
	GENERATED_BODY()

public:
	
	UMyEnemyActorComponent();

	virtual FVector GetFocusLocation() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	EEnemyState CurrentState;

	void SetState(EEnemyState NewState);
	void UpdateIdle(float DeltaTime);
	void UpdateChasing(float DeltaTime);
	void UpdateDashing(float DeltaTime);
	void UpdateAttacking(float DeltaTime);
	void UpdateRecovery(float DeltaTime);


	UPROPERTY(EditAnywhere, Category = "Combat")
	float DashSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ChaseRange = 1500.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float MeleeRange = 500.0f;

	FVector DashTargetLocation;

	

	UFUNCTION(BlueprintCallable, Category = "Combat")
	float ApplyDamage(float DamageAmount);

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FComponentDeadSignature OnUnitDead;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float FireInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HomingStrength = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HomingDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UCameraShakeBase> DamageShakeClass;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UCameraShakeBase> ChronosEndCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	UNiagaraSystem* JitterExplosionComponent;

	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* MeleeAttackRunMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* MeleeAttackMontage_R;  

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* MeleeAttackMontage_L;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* Stamped;


	// ★ 追加：新しい攻撃パターンの窓口
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	UAnimMontage* BackAttackMontage; // その場でのコンボ用

	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	UAnimMontage* BackAttackLandMontage; // その場でのコンボ用

	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	UAnimMontage* AreaBlastMontage;   // 範囲攻撃用

	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	UAnimMontage* SpawnCrystalMagicMontage;   // 範囲攻撃用


	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	UAnimMontage* LauchPad;   

	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	UAnimMontage* DieMontage;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ExecuteAttack(FVector MuzzleLocation, FRotator MuzzleRotation);

	//AreaBlast

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<AMyCrystalMagic>CrystalMagicClass;


	void ExecuteCrystalMagic();
	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 BlastProjectileCount = 12;
	void SpawnAreaBlast();

	void FireRadialProjectiles(float AngleOffset);

	void SpawnSecondWave();
	void SpawnThirdWave(); // ★これが足りなかった！

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PushPlayerAway();
	

	// エディタの詳細パネルから WBP_HealthBar を選択するための変数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UMyEnemyHealthWidget> HealthWidgetClass;

	// 生成したWidgetを保持しておくための変数
	UPROPERTY()
	class UMyEnemyHealthWidget* HealthWidget;



	float ChronosDamageTime = 0.0f;

	// ダメージ窓口
	UFUNCTION()
	void HandleTakeDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	void TickMeleeDetection();

	UFUNCTION(BlueprintCallable)
	void ResetHitActors();

	UPROPERTY(BlueprintReadWrite, Category = "Action")
	bool bIsAttacking;

	UPROPERTY()
	TArray<AActor*> HitActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* EnemyStunMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* HitReactLeftMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* HitReactRightMontage;

	UFUNCTION(BlueprintCallable, Category = "Status")
	void DamageHealth(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnAttackAnimationFinished();




	int32 RemainingJitterCount = 0;

	int32 DefaultRemainingJitterCount = 0;

	UPROPERTY(EditAnywhere, Category = "Chronos")
	float JitterStrength = 300.0f;

	FTimerHandle JitterTimerHandle;

	void ApplyJitterImpulse();

	float GetCurrentHealth();

	float JumpAttackTime = 0.0f;

	UPROPERTY()
	TArray<AActor*>StoredChronosLights;

	AMyCrystalMagic* MagicActor;

protected:
	virtual void BeginPlay() override;


	UPROPERTY(EditAnywhere, Category = "Guard")
	TArray<AActor*> GuardShields;

	TArray<bool> bIsDirectionGuarded;

	EDirectionalSector GetPlayerSector(AActor* PlayerActor);

	void UpdateRandomGuard();
	void PlayParryReaction();



	float LastProjectileTime = -2.0f;
	float RecoveryStartTime;

	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	TArray<UAnimMontage*>MeleeAttackVariants;

	float AttackRangeFar = 500.0f;
	EAttackType NextAttackType;

	bool bHasLaunched;

	bool bJumpAttacked = false;

	bool bFirstLanded = false;
		// バックステップ着地時の待機時間を管理する変数
		float LandTime = -1.0f;

		// 待機時間を調整しやすくするためにプロパティ化（エディタでいじれる！）
		UPROPERTY(EditAnywhere, Category = "AI | Attack")
		float BackStepWaitDuration = 1.0f;

		float AccumulatedDamage = 0.0f;
		void ReleaseAccumulatedDamage();



		UPROPERTY(EditAnywhere, Category = "Effects")
		TSubclassOf<AActor> ChronosLightClass;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
		class USoundBase* ChronosHitSound;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
		class USoundBase* AttackRoarSound;


		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
		class USoundBase* AttackHitSound;
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};