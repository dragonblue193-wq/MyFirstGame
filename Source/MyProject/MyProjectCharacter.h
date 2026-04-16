// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Logging/LogMacros.h"
#include "Engine/OverlapResult.h"
#include "Misc/Optional.h"
#include "Components/TimelineComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "UMyHealthWidget.h" // 先ほど作ったUIのヘッダーをインクルード
#include "MyProjectCharacter.generated.h"

UENUM(BlueprintType)
enum class EPlayerState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Tired UMETA(DisplayName = "Tired"),
	Attacking UMETA(DisplayName = "Attacking"),
	Dodging UMETA(DisplayName = "Dodging"),
	Stunned UMETA(DisplayName = "Stunned"),
	Dead UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class EAttackDirection : uint8
{
	Vertical_Up,
	Vertical_Down,
	Horizontal_L,
	Horizontal_R,
	Neutral
	
};


class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AMyProjectCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;



public:
	AMyProjectCharacter();
	//自分で書いたところ！！！
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	bool IsAttacking;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float Stamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	float MaxStamina = 100.0f;


	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* MyStunMontage;


	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* DeathMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* VerticalDownMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* VerticalUpMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* HorizontalRMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* HorizontalLMontage;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* DefaultComboMontage;

	float MovementInputDuration = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MovementStartDelay = 0.1f; // 0.15秒以上押さないと動かない（数値はお好みで！）


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input)
	FVector2D CurrentMovementInput; // 現在の入力を保持する変数


	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UCameraShakeBase> DamageShakeClass;

	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UCameraShakeBase> ChronosShakeClass;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetHitActors();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DodgeAction;

	void Dodge();
	void CheckParry();
	
	FTimerHandle ParrySlowTimerHandle;
	FTimerHandle ParryTimerHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsDodging = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsParrySuccess = false;


	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bShouldShake = false;


	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bTiredStaminaHeal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chronos")
	bool bIsRestoringTime = false;

	bool bIsWaitingToFinish = false;

	FTimerHandle DodgeTimerHandle; // メンバ変数として定義

	FTimerHandle FinishEffectTimerHandle;

	FTimerHandle CameraTimerHandle;

	UPROPERTY(EditAnywhere, Category = "Camera")
	UCurveFloat* CameraImpactCurve;


	UPROPERTY(EditAnywhere, Category = "Chronos")
	float TimeRestoreSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chronos")
	float RestoreElapsed = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	EAttackDirection CurrentAttackDirection;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	UNiagaraComponent* SwordTrailComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	UNiagaraSystem* TrailSystemAsset;

	UFUNCTION(BlueprintCallable, Category = "VFX")
	void StartSwordTrail();

	UFUNCTION(BlueprintCallable, Category = "VFX")
	void EndSwordTrail();

	void UpdateCameraImpact();

	float ElapsedCameraTime = 0.0f;

	void PerfectDodgeCamera(AActor* OtherActor);
	void StartCameraFocusLookAt(FVector TargetLoc);

	FVector FocusTargetLocation = FVector::ZeroVector;

	UPROPERTY()
	AActor* FocusTargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bIsCameraFocusing = false;

	bool bIsInvincible = false;



protected :
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void BeginPlay() override;

	void Tick(float DeltaTime);
	

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "State") // これを付ける！
	void SetPlayerState(EPlayerState NewState);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	UNiagaraSystem* ParryEffect;

	UFUNCTION()
	void MoveCompleted(const FInputActionInstance& Instance); // 引数を追加
			

protected:

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void NotifyHit(
		class UPrimitiveComponent* MyComp,
		AActor* Other,
		class UPrimitiveComponent* OtherComp,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit
	) override; // ← この override が超重要yp！

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	EPlayerState CurrentState = EPlayerState::Idle;

	UFUNCTION(BlueprintCallable)
	void AttackTrace();

	UFUNCTION(BlueprintCallable)
	void PerformAerialAttackCheck();

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* DodgeMontage;

	// エディタの詳細パネルから WBP_HealthBar を選択するための変数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UMyHealthWidget> HealthWidgetClass;

	// 生成したWidgetを保持しておくための変数
	UPROPERTY()
	class UMyHealthWidget* HealthWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* ChronosSuccessSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* NormalParrySound; // 通常パリー用もついでに！


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* SlashSound; // 通常パリー用もついでに！

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* OnGuardSound; // 通常パリー用もついでに！

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* ParryVoiceSound; // 通常パリー用もついでに！

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* AttackVoiceSound; // 通常パリー用もついでに！

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* DamageSound; // 通常パリー用もついでに！


	void OnParried(FVector ImpactPoint); // 弾かれた時の汎用関数



protected :
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LockOnAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Status)
	bool bIsLockOn = false;

	void LockOnStarted();
	void LockOnCompleted();


	UFUNCTION(BlueprintCallable)
	void DetermineAttackDirection();

	UFUNCTION(BlueprintCallable)
	void ExecuteDirectionalAttack(EAttackDirection Direction);

	// パリーのスロー演出を管理するタイマーハンドル
	FTimerHandle RestoreTimeTimerHandle;


	float LastStaminaUseTime;
	UPROPERTY(EditAnywhere, Category = "Status")
	float StaminaRegenDelay = 0.5f;


	UPROPERTY(EditAnywhere, Category = "Chronos")
	float RestoreDuration = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chronos")
	float ExecutionWatchTime = 0.2f;
    
	UFUNCTION(BlueprintCallable)
	void FinishChronosEffect(); // ← これを追加！

	// 元のFOV（通常90）とカメラ距離（通常400）を保持する変数
	float DefaultFOV = 90.0f;
	float DefaultArmLength = 450.0f;

	UPROPERTY()
	AActor* ParriedEnemy;

	// 演出用の回転（保存用）
	FRotator TargetRotation;


	bool bIsWatchingExecution = false;

	// 1. WB_GAMEOVER をセットするためのクラス
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> GameOverWidgetClass;

	// 2. 生成したインスタンスを保持
	UPROPERTY()
	class UUserWidget* GameOverWidget;

	// 3. ボタンが押された時に呼ばれる関数（UFUNCTION必須！）
	UFUNCTION(BlueprintCallable, Category = "UI") // ★ BlueprintCallable を追加！
	void OnRestartButtonClicked();

	void BimdWidgetButtons();

	

	// 1. エディタから選択できるようにするWidgetクラス
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> StartMenuWidgetClass;

	// 2. 生成したWidgetを保持する変数
	UPROPERTY()
	UUserWidget* CurrentWidget;

	// 1. エディタから選択できるようにするWidgetクラス
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> EndMenuWidgetClass;

	// 2. 生成したWidgetを保持する変数
	UPROPERTY()
	UUserWidget* EndWidget;

	UFUNCTION()
	void  ClearGameWidget();

	UFUNCTION(BlueprintCallable)
	void TransitionToGame();

private :
	UPROPERTY()
	TSet<AActor*> HitActors;
};

