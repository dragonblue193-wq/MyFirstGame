// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "MyEnemyActorComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/AudioComponent.h" // ← これを追加！
#include "InputActionValue.h"
#include "InputAction.h" //← これが特に重要！
#include "Kismet/KismetMathLibrary.h" // これを執念で追加！
#include "BPI_Parryable.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h" // これも追加！
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceSkeletalMesh.h" // ← これ追加
#include "DrawDebugHelpers.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
DEFINE_LOG_CATEGORY(LogTemplateCharacter);




//////////////////////////////////////////////////////////////////////////
// AMyProjectCharacter

AMyProjectCharacter::AMyProjectCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMyProjectCharacter::SetPlayerState(EPlayerState NewState)
{
	if (CurrentState == NewState) return;

	 CurrentState = NewState;

	 // これをログに出して、歩く前に Idle になってないかチェック！
	 UE_LOG(LogTemp, Warning, TEXT("STATE CHANGED TO: %d"), (int)NewState);


}

//////////////////////////////////////////////////////////////////////////
// Input
void AMyProjectCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentState = EPlayerState::Idle;
	

	// PlayerControllerを取得
	APlayerController* PC = Cast<APlayerController>(GetController());

	if (HealthWidgetClass && PC)
	{
		// 第一引数を GetWorld() に変更
		HealthWidget = CreateWidget<UMyHealthWidget>(GetWorld(), HealthWidgetClass);

		if (HealthWidget)
		{
			HealthWidget->AddToViewport();
			HealthWidget->UpdateHealthPercent(Health, 100.0f);
			HealthWidget->UpdateStaminaPercent(Stamina, MaxStamina);
		}
	}

	if (StartMenuWidgetClass && PC) // PC（PlayerController）が有効であることを確認
	{
		// 第1引数を this ではなく PC に変更します
		CurrentWidget = CreateWidget<UUserWidget>(PC, StartMenuWidgetClass);

		if (CurrentWidget)
		{
			CurrentWidget->AddToViewport();

			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(CurrentWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
	}

	// 1. 世界の中から ACharacter (または AActor) を探す
	TArray<AActor*> OutActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), OutActors);

	for (AActor* Actor : OutActors)
	{
		// 2. 目的のコンポーネントを持っているかチェック
		UMyEnemyActorComponent* EnemyComp = Actor->FindComponentByClass<UMyEnemyActorComponent>();

		if (EnemyComp)
		{
			// 3. 見つけたら即座にバインドしてループ終了
			EnemyComp->OnUnitDead.AddDynamic(this, &AMyProjectCharacter::ClearGameWidget);
			break;
		}
	}

}

void AMyProjectCharacter::ClearGameWidget()
{
	if (EndMenuWidgetClass)
	{
		 EndWidget = CreateWidget<UUserWidget>(GetWorld(), EndMenuWidgetClass);
		if (EndWidget)
		{
			EndWidget->AddToViewport();
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (PC)
			{
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(EndWidget->TakeWidget());
				PC->SetInputMode(InputMode);
				PC->bShowMouseCursor = true;
				UE_LOG(LogTemp, Warning, TEXT("Enemy Component Bound Successfully!"));
			}
		}
	}
}

void AMyProjectCharacter::TransitionToGame()
{
	APlayerController* PC = Cast<APlayerController>(GetController());

	if (PC)
	{
		FInputModeGameOnly GameMode;
		PC->SetInputMode(GameMode); // PC-> を追加
		PC->bShowMouseCursor = false; // PC-> を追加
	}

	// 必要に応じてWidgetを消去
	if (CurrentWidget)
	{
		CurrentWidget->RemoveFromParent();
	}
}


void AMyProjectCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentMovementInput.Size() > 0.5f)
	{
		MovementInputDuration += DeltaTime;
	}
	else
	{
		MovementInputDuration = 0.0f;
	}


	
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float WorldTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld());

	if (CurrentState != EPlayerState::Tired && Stamina <= 0.0f && WorldTimeScale >= 1.0f)
	{
		SetPlayerState(EPlayerState::Tired);
		GetCharacterMovement()->MaxWalkSpeed = 100.0f;
		bTiredStaminaHeal = true;
	}
	else if (CurrentState == EPlayerState::Tired && Stamina >= 100.0f)
	{
		SetPlayerState(EPlayerState::Idle);
		GetCharacterMovement()->MaxWalkSpeed = 500.0f;
		bTiredStaminaHeal = false;
	}

		if (Stamina < MaxStamina && (CurrentTime - LastStaminaUseTime) >= StaminaRegenDelay && CurrentState != EPlayerState::Tired)
	    {
		   Stamina = FMath::Min(MaxStamina, Stamina + (7.0f * DeltaTime));
		   if (HealthWidget)
		   {
			   HealthWidget->UpdateStaminaPercent(Stamina, MaxStamina);
		   }
	     }
		else if (Stamina < MaxStamina && (CurrentTime - LastStaminaUseTime) >= StaminaRegenDelay && Stamina < MaxStamina && CurrentState == EPlayerState::Tired)
		{
			Stamina = FMath::Min(MaxStamina, Stamina + (30.0f * DeltaTime));
			if (HealthWidget)
			{
				HealthWidget->UpdateStaminaPercent(Stamina, MaxStamina);
			}
		}
		

		
		if (bIsRestoringTime)
		{
			CustomTimeDilation = 1.0f;
			RestoreElapsed += FApp::GetFixedDeltaTime();;

			// --- A. 鑑賞フェーズ（まだ ExecutionWatchTime に達していない時） ---
			if (RestoreElapsed <= ExecutionWatchTime)
			{
				// 敵に密着したまま固定（時間はスローのまま）
				UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.5f);

				if (CameraBoom && FollowCamera)
				{
					CameraBoom->TargetArmLength = 1500.0f; // 密着距離
					FollowCamera->SetFieldOfView(70.0f); // ズーム
					FollowCamera->PostProcessSettings.ColorSaturation = FVector4(0, 0, 0, 1); // 白黒維持
				}
			}
			// --- B. 復帰フェーズ（鑑賞時間を過ぎた後） ---
			else
			{
				// 鑑賞時間を引いた後の「戻り専用」の進捗率を計算
				float RecoveryAlpha = FMath::Clamp((RestoreElapsed - ExecutionWatchTime) / RestoreDuration, 0.0f, 1.0f);

				// 時間速度を戻す
				float NewGTD = FMath::Lerp(0.5f, 1.0f, RecoveryAlpha);
				UGameplayStatics::SetGlobalTimeDilation(GetWorld(), NewGTD);

				if (CameraBoom && FollowCamera)
				{
					// [ 80.0 ] ────> [ Default ]
					//CameraBoom->TargetArmLength = FMath::Lerp(1500.0f, 1.5f * DefaultArmLength, RecoveryAlpha);
					// [ 30.0 ] ────> [ Default ]
					FollowCamera->SetFieldOfView(FMath::Lerp(70.0f, DefaultFOV, RecoveryAlpha));
					// [ 白黒 ] ────> [ カラー ]
					FollowCamera->PostProcessSettings.ColorSaturation = FVector4(RecoveryAlpha, RecoveryAlpha, RecoveryAlpha, 1.0f);
				}

				if (RecoveryAlpha >= 1.0f)
				{
					// 二重タイマー防止用のフラグ（ヘッダで bool bIsWaitingToFinish; を定義推奨）
					if (!bIsWaitingToFinish)
					{
						bIsWaitingToFinish = true;
						float DelayTime = 0.0f;

						if (ParriedEnemy)
						{
							if (UMyEnemyActorComponent* EnemyComp = ParriedEnemy->FindComponentByClass<UMyEnemyActorComponent>())
							{
								DelayTime = EnemyComp->StoredChronosLights.Num() * 0.1f + 0.5f;
							}
						}

						if (DelayTime > 0.0f)
						{
							GetWorldTimerManager().SetTimer(FinishEffectTimerHandle, this, &AMyProjectCharacter::FinishChronosEffect, DelayTime, false, -1.0f);
						}
						else
						{
							// 遅延が必要ないなら即終了
							FinishChronosEffect();
						}
					}
				}
			}
		}

		if (bIsCameraFocusing && IsValid(FocusTargetActor))
		{
			FVector StartLoc = GetPawnViewLocation();
			UMyEnemyActorComponent* EnemyComp = FocusTargetActor->FindComponentByClass<UMyEnemyActorComponent>();
			if (EnemyComp)
			{
				FVector TargetLoc = EnemyComp->GetFocusLocation();
				FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, TargetLoc);
				if (APlayerController* PC = Cast<APlayerController>(GetController()))
				{
					FRotator CurrentRot = PC->GetControlRotation();
					FRotator SmoothedRot = FMath::RInterpTo(CurrentRot, LookAtRot, GetWorld()->GetDeltaSeconds(), 60.0f);
					PC->SetControlRotation(SmoothedRot);
				}
			}
		}
		else {
			if (FocusTargetActor)
			{
				//UE_LOG(LogTemp, Error, TEXT("EnemyCompが見つからないぞ！相手の名前: %s"), *FocusTargetActor->GetName());
			}
			else
			{
				//UE_LOG(LogTemp, Error, TEXT("FocusTargetActorがnullptrだぞ！"));
			}
		}
			
		
}
void AMyProjectCharacter::FinishChronosEffect()
{


	bIsRestoringTime = false;
	RestoreElapsed = 0.0f;
	CustomTimeDilation = 1.0f;
	ParriedEnemy->CustomTimeDilation = 1.0f;

	// 2. カメラを自分（カプセル）に戻す
	if (CameraBoom)
	{
		CameraBoom->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		CameraBoom->SetRelativeLocation(FVector(0, 0, 100.0f)); // 自分の肩あたりの高さに
		CameraBoom->TargetArmLength = DefaultArmLength;
		CameraBoom->SocketOffset = FVector::ZeroVector;
	}

	// 3. FOVと時間の速度を完全に元に戻す
	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(DefaultFOV);
		// ポストプロセスの上書きを解除
		FollowCamera->PostProcessSettings.bOverride_ColorSaturation = false;
		FollowCamera->PostProcessSettings.bOverride_ColorContrast = false;
	}

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

	UE_LOG(LogTemp, Warning, TEXT("Chronos Effect Finished!"));

	bIsWaitingToFinish = false;
}
	





void AMyProjectCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Move);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AMyProjectCharacter::MoveCompleted);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AMyProjectCharacter::MoveCompleted);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Look);

		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &AMyProjectCharacter::LockOnStarted);
		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Completed, this, &AMyProjectCharacter::LockOnCompleted);

		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AMyProjectCharacter::Dodge);

	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMyProjectCharacter::DetermineAttackDirection()
{
	EAttackDirection ChosenDir = EAttackDirection::Neutral;
	const float Deadzone = 0.2f;

	// ★ 追加：入力の「合計値（Size）」がデッドゾーンを超えている時だけ判定
	if (CurrentMovementInput.Size() >= Deadzone)
	{
		if (FMath::Abs(CurrentMovementInput.Y) >= FMath::Abs(CurrentMovementInput.X))
		{
			// Wなら下斬り、Sなら上斬り（めておさんの既存ロジック通り）CurrentAttackDirection
			ChosenDir = (CurrentMovementInput.Y > 0) ? EAttackDirection::Vertical_Down : EAttackDirection::Vertical_Up;
		}
		else
		{
			// Dなら右斬り、Aなら左斬り
			ChosenDir = (CurrentMovementInput.X > 0) ? EAttackDirection::Horizontal_R : EAttackDirection::Horizontal_L;
		}
	}
	else
	{
		// 何も押していなければニュートラル攻撃
		ChosenDir = EAttackDirection::Vertical_Down;
	}

	ExecuteDirectionalAttack(ChosenDir);
}

void AMyProjectCharacter::ExecuteDirectionalAttack(EAttackDirection Direction)
{
	UAnimMontage* MontageToPlay = nullptr;

	switch (Direction)
	{
	case EAttackDirection::Vertical_Down: MontageToPlay = VerticalDownMontage; break;
	case EAttackDirection::Vertical_Up:   MontageToPlay = VerticalUpMontage;   break;
	case EAttackDirection::Horizontal_R:  MontageToPlay = HorizontalRMontage;  break;
	case EAttackDirection::Horizontal_L:  MontageToPlay = HorizontalLMontage;  break;
	case EAttackDirection::Neutral:      // 追記：入力がない時の標準攻撃
	default:
		MontageToPlay = DefaultComboMontage;
		break;
	}


	CurrentAttackDirection = Direction;
	if (MontageToPlay) PlayAnimMontage(MontageToPlay);
}

void AMyProjectCharacter::LockOnStarted()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		// 物理的にキーが押されているかチェック
		bool bIsShiftDown = PC->IsInputKeyDown(EKeys::LeftShift);

		// 物理的に押されていないなら、ここで処理を終了（return）させてフラグを立てさせない
		if (!bIsShiftDown)
		{
			UE_LOG(LogTemp, Warning, TEXT("LockOnStarted BLOCKED: Phantom input detected."));
			return;
		}
	}

	// 物理的に押されている場合のみ、以下の処理が実行される
	bIsLockOn = true;
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	UE_LOG(LogTemp, Warning, TEXT("LockOnStarted! (Confirmed by Physical Key)"));
}
void AMyProjectCharacter::LockOnCompleted()
{
	bIsLockOn = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AMyProjectCharacter::Move(const FInputActionValue& Value)
{
	if (CurrentState != EPlayerState::Idle && CurrentState != EPlayerState::Tired)
	{
		return;
	}

	FVector2D MovementVector = Value.Get<FVector2D>();
	CurrentMovementInput = MovementVector; // 判定用には常に保存

	// --- 追加：入力時間のカウントと判定 ---
	if (!MovementVector.IsNearlyZero())
	{
		// Tickで加算しても良いですが、Moveが呼ばれている間加算する方式
		// 正確な秒数にしたい場合は、Tickで MovementInputDuration += DeltaTime; を推奨
	}
	else
	{
		MovementInputDuration = 0.0f;
	}

	if (Controller != nullptr)
	{
		// ★ ここで「一定時間以上押しているか」をチェック！
		if (MovementInputDuration >= MovementStartDelay)
		{
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			AddMovementInput(ForwardDirection, MovementVector.Y);
			AddMovementInput(RightDirection, MovementVector.X);
		}
	}

	// --- 既存のロックオン切り替え処理（ここは常に実行させる） ---
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		if (PC->IsInputKeyDown(EKeys::LeftShift))
		{
			bIsLockOn = true;
			bUseControllerRotationYaw = true;
			GetCharacterMovement()->bOrientRotationToMovement = false;
		}
		else
		{
			bIsLockOn = false;
			bUseControllerRotationYaw = false;
			GetCharacterMovement()->bOrientRotationToMovement = true;
		}
	}
}

void AMyProjectCharacter::MoveCompleted(const FInputActionInstance& Instance)
{
	// クラスのメンバ変数にアクセス
	CurrentMovementInput = FVector2D::ZeroVector;
	MovementInputDuration = 0.0f;
}

void AMyProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
// 2. 衝突時の処理（NotifyHitをオーバーライドするのが一番楽ですyp！）
// 衝突時の処理
void AMyProjectCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	if (IsAttacking)
	{
		if (Other && Other != this)
		{
			
			UMyEnemyActorComponent* EnemyComp = Other->FindComponentByClass<UMyEnemyActorComponent>();

			if (EnemyComp)
			{
				// 2. コンポーネントの関数を直接叩いてダメージを与える
				//EnemyComp->DamageHealth(10.0f);

				
				//UE_LOG(LogTemp, Warning, TEXT("C++ Component Damage Applied: 10"));
			}
		}
	}
}
void AMyProjectCharacter::AttackTrace()
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.5f)
	{
		if (AttackVoiceSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, AttackVoiceSound, GetActorLocation());
		}
	}
	StartSwordTrail();
	
	// 1. 判定の中心を計算（剣の「根元」と「先端」の真ん中）
	FVector Start = GetMesh()->GetSocketLocation("Start");
	FVector End = GetMesh()->GetSocketLocation("End");
	FVector Center = (Start + End) * 0.5f;

	// 判定用の球体サイズ（剣の長さに合わせて 80.0f ～ 120.0f くらいで調整）
	float Radius = 100.0f;

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // 自分は無視

	// ★ Sweep ではなく Overlap を「この瞬間だけ」実行
	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		Center,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(Radius),
		Params
	);

	if (bHit)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* HitActor = Result.GetActor();
			if (!HitActor) continue;

			// 敵のコンポーネントを取得
			UMyEnemyActorComponent* EnemyComp = HitActor->FindComponentByClass<UMyEnemyActorComponent>();
			if (EnemyComp)
			{

				float OldHealth = EnemyComp->GetCurrentHealth();

				// --- [ 1. ダメージ適用 ] ---
				// 1回しか呼ばれないので、ガード判定も1回しか走りません
				EnemyComp->DamageHealth(15.0f);

				float NewHealth = EnemyComp->GetCurrentHealth();

				// --- [ 2. サウンド演出 ] ---
				if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) < 0.5f)
				{
					UAudioComponent* AudioComp = UGameplayStatics::SpawnSoundAtLocation(this, SlashSound, GetActorLocation(), FRotator::ZeroRotator);

					if (AudioComp)
					{
						// 再生速度（ピッチ）を 0.5倍 に設定
						// ※UEではピッチを下げると再生速度も比例して遅くなります
						AudioComp->SetPitchMultiplier(0.8f);
					}
					
					if (ChronosShakeClass)
					{
						if (APlayerController* PC = Cast<APlayerController>(GetController()))
						{
							PC->ClientStartCameraShake(ChronosShakeClass);
						}
					}
				}
				else
				{
					if (NewHealth < OldHealth)
					{
						// ★【肉を切る音】HPが減った ＝ ガードされなかった！
						if (SlashSound)
						{
							UGameplayStatics::PlaySoundAtLocation(this, SlashSound, GetActorLocation());
						}

						// --- [ 3. ヒットストップ（執念の実装） ] ---
				// 現在の世界の速度をバックアップ
						float OriginalGlobalTime = UGameplayStatics::GetGlobalTimeDilation(GetWorld());

						// 世界をほぼ止める（0.01倍速）
						UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.01f);

						// 指定時間後に元の速度に戻すタイマーをセット
						FTimerHandle HitStopHandle;
						// 第3引数の 0.00015f は「現実世界の秒数」として処理させる
						GetWorldTimerManager().SetTimer(HitStopHandle, [OriginalGlobalTime, this]()
							{
								// [ 0.01f ] ────> [ 元の速度 ] に復帰
								UGameplayStatics::SetGlobalTimeDilation(GetWorld(), OriginalGlobalTime);
							}, 0.0015f, false, -1.0f); // ★最後の -1.0f が「実時間計測」のキモ！
					}
					else
					{
						// ★【金属音】HPが減っていない ＝ ガードされた！
						if (OnGuardSound) // ヘッダで定義したガード用の音
						{
							UGameplayStatics::PlaySoundAtLocation(this, OnGuardSound, GetActorLocation());
						}
					}
				}


				// --- [ 4. カメラシェイク ] ---
				if (DamageShakeClass)
				{
					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						PC->ClientStartCameraShake(DamageShakeClass);
					}
				}

				// 1スイングで1体にしか当てたくない場合はここで抜ける
				// 複数をまとめて斬りたいならこの break を消す
				break;
			}
		}
	}

	// デバッグ表示（エディタで範囲を確認したいならコメントアウト解除）
	// DrawDebugSphere(GetWorld(), Center, Radius, 12, FColor::Green, false, 0.5f);
}
void AMyProjectCharacter::StartSwordTrail()
{
	if (!TrailSystemAsset)
	{
		return;
	}
	if (SwordTrailComponent)
	{
		SwordTrailComponent->Deactivate();
	}

	SwordTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		TrailSystemAsset,
		GetMesh(),
		FName("ik_foot_root_weapon_r"), // ← ここを FName() で囲う！
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true
	);

	if (SwordTrailComponent)
	{
		//SwordTrailComponent->SetVariableFName(FName(TEXT("User.StartSocket")), FName(TEXT("Start")));
		//SwordTrailComponent->SetVariableFName(FName(TEXT("User.EndSocket")), FName(TEXT("End")));

		SwordTrailComponent->Activate();
	}
}

void AMyProjectCharacter::EndSwordTrail()
{
	if (SwordTrailComponent)
	{
		// 即座に消すと不自然なので、Deactivateで寿命が尽きるのを待つ（商学的なアフターケア）
		SwordTrailComponent->Deactivate();
	}
}



void AMyProjectCharacter::PerformAerialAttackCheck()
{
	float AttackRange = 250.0f;
	float CapsuleRadius = 150.0f;
	float CapsuleHalfHeight = 200.0f;
	float LeftOffset = 50.0f;

	FVector OffsetVector = GetActorRightVector() * -LeftOffset;

	FVector StartLocation = GetActorLocation() + OffsetVector;
	FVector EndLocation = StartLocation + (GetActorForwardVector() * AttackRange);

	FCollisionShape  AttackCapsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);


	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		AttackCapsule,
		QueryParams
	);

	//DrawDebugCapsule(GetWorld(), (StartLocation + EndLocation) * 0.5f + OffsetVector, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Red, false, 5.0f);
		if (bHit)
		{
			for (auto& Hit : HitResults)
			{

				AActor* HitActor = Hit.GetActor();
				if (!HitActor || HitActors.Contains(HitActor))continue;

				UMyEnemyActorComponent* EnemyComp = HitActor->FindComponentByClass<UMyEnemyActorComponent>();

				if (EnemyComp)
				{
					HitActors.Add(HitActor);
					EnemyComp->DamageHealth(50.0f);
				}
			}
		}

}

void AMyProjectCharacter::ResetHitActors()
{
	HitActors.Empty();
}


void AMyProjectCharacter::Dodge()
{

	UE_LOG(LogTemp, Warning, TEXT("Dodge Function Called!")); // ← これが出るか確認

	if (CurrentState != EPlayerState::Idle || GetCharacterMovement()->IsFalling())return;

	if (Stamina < 0.0f)
	{
		return;
	}

	if (ParryVoiceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ParryVoiceSound, GetActorLocation());
	}

	Stamina -= 30.0f;
	LastStaminaUseTime = GetWorld()->GetTimeSeconds();

	if (Stamina < 0.0f)
	{
		Stamina = 0.0f;
	}

	if (HealthWidget)
	{
		HealthWidget->UpdateStaminaPercent(Stamina, MaxStamina);
	}

	FVector DodgeDirection = GetCharacterMovement()->GetLastInputVector();

	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = GetActorForwardVector();
	}
	DodgeDirection = DodgeDirection.GetSafeNormal(); // 追加

	SetPlayerState(EPlayerState::Dodging);
	bIsParrySuccess = false;
	HitActors.Empty();

	GetCharacterMovement()->StopMovementImmediately();

	float DodgeStrength = 1500.0f;

	// 進行方向に向きをセット（瞬時）
	if (!DodgeDirection.IsNearlyZero())
	{
		SetActorRotation(DodgeDirection.Rotation());
	}
	LaunchCharacter(DodgeDirection * DodgeStrength, true, true);

	if (DodgeMontage)
	{
		PlayAnimMontage(DodgeMontage);
	}

	CheckParry();
	if (GetWorld()->GetTimeSeconds() != 0)
	{
		if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) >= 1.0f)
		{
			GetWorldTimerManager().SetTimer(ParrySlowTimerHandle, this, &AMyProjectCharacter::CheckParry, 0.05f, false);
		}
	}
	
	

}
void AMyProjectCharacter::CheckParry()
{
	TArray<FHitResult> OutHits;
	FVector SweepStart = GetActorLocation();
	FVector SweepEnd = SweepStart;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionShape MySphere = FCollisionShape::MakeSphere(200.0f);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);




	bool bHit = GetWorld()->SweepMultiByObjectType(OutHits, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, MySphere, QueryParams);

	if (!bHit)
	{
		// ヒットしなかった場合：一定時間後にIdleに戻る
		if (!GetWorldTimerManager().IsTimerActive(DodgeTimerHandle))
		{
			GetWorldTimerManager().SetTimer(DodgeTimerHandle, [this]() { 
				
				if (bTiredStaminaHeal)
				{
					SetPlayerState(EPlayerState::Tired);
				}
				else
				{
					SetPlayerState(EPlayerState::Idle);
				}
				
				}, 0.9f, false);
		}
		return;
	}

	for (auto& Hit : OutHits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || !HitActor->GetClass()->ImplementsInterface(UBPI_Parryable::StaticClass())) continue;

		UMyEnemyActorComponent* EnemyComp = HitActor->FindComponentByClass<UMyEnemyActorComponent>();
		if (EnemyComp && !EnemyComp->bIsAttacking) continue;

		// --- ここからパリー成功確定の共通処理 ---
		ParriedEnemy = HitActor;
		PerfectDodgeCamera(HitActor);
		bIsParrySuccess = true;
		bIsDodging = false;
		GetWorldTimerManager().ClearTimer(DodgeTimerHandle);

		// パリーイベント通知 & エフェクト
		IBPI_Parryable::Execute_OnParried(HitActor, this);
		if (ParryEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ParryEffect, GetActorLocation(), GetActorRotation());
		}

		// カプセル衝突の一時無視（弾除け）
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		GetWorldTimerManager().SetTimer(ParryTimerHandle, [this]() {
			GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
			}, 0.9f, false);

		// --- 特殊分岐：クロノスか通常か ---
		if (Stamina >= MaxStamina - 30.0f)
		{
			// 【クロノス・パリー】
			UE_LOG(LogTemp, Warning, TEXT("CHRONOS PARRY!!!"));

			// 1. 世界を止める
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.005f);

			if (ChronosSuccessSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, ChronosSuccessSound, GetActorLocation());
			}
			if (NormalParrySound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, NormalParrySound, GetActorLocation());
			}


			

			// 2. 自分は「スロー」に見える程度に設定（200倍だと早すぎる）
			// 1.0 にすると自分もほぼ止まり、20.0 くらいだと「自分だけ少し動ける」感が出ます
			CustomTimeDilation = 10.0f;
			HitActor->CustomTimeDilation = 4.0f;

			FTimerHandle ChronosTimerHandle1;
			// タイマーを 3.0f (実時間) に伸ばして、しっかり停止時間を確保する
			GetWorldTimerManager().SetTimer(ChronosTimerHandle1, [this]() {

				CustomTimeDilation = 300.0f;
				bIsCameraFocusing = false;
				
				}, 0.005f, false, -1.0f); // -1.0f で実時間の3秒にする


			if (FollowCamera && CameraBoom)
			{

				DefaultFOV = FollowCamera->FieldOfView;
				DefaultArmLength = CameraBoom->TargetArmLength;

				FollowCamera->SetFieldOfView(120.0f);
				CameraBoom->TargetArmLength = 300.0f;
				
				// 1. もし古いタイマーが生きていたら、強制終了（二重起動防止）
				//GetWorldTimerManager().ClearTimer(CameraTimerHandle);

				// 2. 経過時間を 0.0 にリセット（ここが生命線！）
				//ElapsedCameraTime = 0.0f;

				// 3. タイマー起動！ 0.01秒ごとに UpdateCameraImpact を呼ぶ
				//GetWorldTimerManager().SetTimer(
					//CameraTimerHandle,
					//this,
					//&AMyProjectCharacter::UpdateCameraImpact,
					//0.00001f,
					//true
				//);

				// 彩度(Saturation)を 0.0 にすると白黒になります
				FollowCamera->PostProcessSettings.bOverride_ColorSaturation = true;
				FollowCamera->PostProcessSettings.ColorSaturation = FVector4(0.0f, 0.0f, 0.0f, 1.0f);

				// ついでにコントラストを少し上げると、より「劇画チック」で格好良くなります
				FollowCamera->PostProcessSettings.bOverride_ColorContrast = true;
				FollowCamera->PostProcessSettings.ColorContrast = FVector4(1.2f, 1.2f, 1.2f, 1.0f);
			}


			FTimerHandle ChronosWatchingEnemyHandle;
			// --- ChronosWatchingEnemyHandle (0.02sタイマー) の中身を以下のように書き換え ---
			GetWorldTimerManager().SetTimer(ChronosWatchingEnemyHandle, [this]() {

				if (CameraBoom && FollowCamera && ParriedEnemy)
				{
					// 1. 敵をACharacterにキャストしてMeshを取得
					if (ACharacter* EnemyChar = Cast<ACharacter>(ParriedEnemy))
					{
						// 敵の胸(spine_03)などにアタッチ
						CameraBoom->AttachToComponent(EnemyChar->GetMesh(),
							FAttachmentTransformRules::SnapToTargetNotIncludingScale,
							FName("loin_cloth_fr_l_01"));

						// 2. 敵中心のカメラワークを設定（相対位置）
						CameraBoom->TargetArmLength = 1500.0f;
						CameraBoom->SocketOffset = FVector(0, 0, 75.0f);

						
						// 3. 敵の方を向かせる
						FVector LookAtPos = ParriedEnemy->GetActorLocation();

						// 2. カメラが「どこから見ている体にするか」の基準点（自分の少し上空）
						FVector HighViewPoint = GetActorLocation() + FVector(0, 0, 100.0f);

						// 3. 【重要】(ゴール - スタート) の順で引く！
						// 「上空(HighViewPoint) から 敵(LookAtPos) を見た時」の回転を計算
						FRotator TargetRot = (LookAtPos - HighViewPoint).Rotation();
						if (Controller)
						{
							Controller->SetControlRotation(TargetRot);
						}

						// 視野角を絞って「鑑賞モード」
						FollowCamera->SetFieldOfView(50.0f);
					} // ← ここでEnemyCharのifを閉じる
				}
				}, 0.01f, false, -1.0f);

			FTimerHandle ChronosTimerHandle;
			
			GetWorldTimerManager().SetTimer(ChronosTimerHandle, [this]() {
				
				RestoreElapsed = 0.0f;

				bIsRestoringTime = true;


				bTiredStaminaHeal = false;
				GetCharacterMovement()->MaxWalkSpeed = 500.0f;
				GetCharacterMovement()->UpdateComponentVelocity();

				if (FollowCamera)
				{
					// 上書き設定をオフにするだけで元通りになります
					//FollowCamera->PostProcessSettings.bOverride_ColorSaturation = false;
					//FollowCamera->PostProcessSettings.bOverride_ColorContrast = false;
				}
				}, 0.03f, false, -1.0f); 

			Stamina = 0.1f;
			//SetPlayerState(EPlayerState::Idle);
			if (HealthWidget) HealthWidget->UpdateStaminaPercent(Stamina, MaxStamina);

		}
		else
		{
			// 【通常パリー】
			Stamina = FMath::Min(MaxStamina, Stamina + 50.0f);
			

			if (NormalParrySound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, NormalParrySound, GetActorLocation());
			}

			bIsInvincible = true;

			bTiredStaminaHeal = false;
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.1f);
			CustomTimeDilation = 10.0f;

			GetWorldTimerManager().SetTimer(RestoreTimeTimerHandle, [this]() {
				UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
				CustomTimeDilation = 1.0f;
				}, 0.05f, false);


			
		}


		FTimerHandle InvincibleTimer;
		GetWorldTimerManager().SetTimer(InvincibleTimer, [this]() {

			bIsInvincible = false;
			}, 0.9f, false);



		// 共通：ステータスをIdleに戻す予約
		GetWorldTimerManager().SetTimer(DodgeTimerHandle, [this]() {
			
			if (bTiredStaminaHeal)
			{
				SetPlayerState(EPlayerState::Tired);
			}
			else
			{
				SetPlayerState(EPlayerState::Idle);
			}


			}, 0.005f, false);

		if (HealthWidget) HealthWidget->UpdateStaminaPercent(Stamina, MaxStamina);

		break; // 1体パリーしたらループ終了
	}
}

void AMyProjectCharacter::PerfectDodgeCamera(AActor* OtherActor)
{
	// 1. ここまでは来ている（ログで確認済み）
	UE_LOG(LogTemp, Warning, TEXT("PerfectDodgeCamera called with: %s"), OtherActor ? *OtherActor->GetName() : TEXT("NULL"));

	if (!OtherActor) return;

	// 2. 【重要】キャストの「外」で代入する！
	FocusTargetActor = OtherActor;
	bIsCameraFocusing = true;

	FTimerHandle FocusResetHandle;
	GetWorld()->GetTimerManager().SetTimer(FocusResetHandle, [this]() {
		bIsCameraFocusing = false;
		}, 0.05f, false);

	// 3. インターフェースのチェックは「おまけ」として扱う
	ICombatInterface* DirectInterface = Cast<ICombatInterface>(OtherActor);
	if (DirectInterface)
	{
		UE_LOG(LogTemp, Warning, TEXT("Success: CombatInterface detected!"));
	}
	else
	{
		// おそらくここを通ります。でも FocusTargetActor には代入されているので OK yp!
		UE_LOG(LogTemp, Error, TEXT("Note: %s does not have C++ CombatInterface, but following anyway."), *OtherActor->GetName());
	}
	
}



void AMyProjectCharacter::UpdateCameraImpact()
{
	// [ Current ] ────> [ Target ]
	float Duration = 0.3f; // 納期（秒）
	ElapsedCameraTime += 0.01f; // タイマーの間隔分足していく
	float Alpha = FMath::Clamp(ElapsedCameraTime / Duration, 0.0f, 1.0f);

	CameraBoom->TargetArmLength = FMath::Lerp(450.0f, 250.0f, Alpha);

	if (Alpha >= 1.0f) {
		GetWorldTimerManager().ClearTimer(CameraTimerHandle);
	}
}
void AMyProjectCharacter::OnParried(FVector SfxLocation)
{
	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (!AnimInst) return;

	UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage();
	if (CurrentMontage)
	{
		float PlayRate = -1.5f;

		
			if (OnGuardSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, OnGuardSound, GetActorLocation());
			}

		AnimInst->Montage_SetPlayRate(CurrentMontage, PlayRate);

		float TimeToWait = AnimInst->Montage_GetPosition(CurrentMontage) / FMath::Abs(PlayRate);

		FTimerHandle IdleTimer;
		// [ TTimerDelegate ] ────> [ Lambda ] ────> [ thisを安全にキャプチャ ]
		GetWorld()->GetTimerManager().SetTimer(IdleTimer, FTimerDelegate::CreateLambda([this]() {
			if (this) StopAnimMontage(nullptr);
			SetPlayerState(EPlayerState::Idle);
			}), TimeToWait, false);
	}
}

float AMyProjectCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{

	if (CurrentState == EPlayerState::Dodging || bIsRestoringTime || UGameplayStatics::GetGlobalTimeDilation(GetWorld()) < 0.2f || bIsInvincible)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dodged! No Damage."));
		return 0.0f;
	}
	if(DamageSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DamageSound, GetActorLocation());
	}

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health -= ActualDamage;

	bIsInvincible = true;

	SetPlayerState(EPlayerState::Stunned);

	// スタンモーション（自分自身のMeshからAnimInstanceを取る）
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && MyStunMontage)
	{
		AnimInstance->Montage_Play(MyStunMontage);
	}
	
	if (DamageShakeClass)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC && PC->PlayerCameraManager)
		{
			PC->ClientStartCameraShake(DamageShakeClass);
		}
	}

	if (HealthWidget)
	{
		HealthWidget->UpdateHealthPercent(Health, 100.0f);
	}


	CustomTimeDilation = 0.1f;
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, [this]()
		{
			CustomTimeDilation = 1.0f;
		}, 0.1f, false, -1.f);

	FTimerHandle StunTimer;
	GetWorldTimerManager().SetTimer(StunTimer, [this]() {
		if (CurrentState == EPlayerState::Stunned)
		{
			if (bTiredStaminaHeal)
			{
				SetPlayerState(EPlayerState::Tired);
			}
			else
			{
				SetPlayerState(EPlayerState::Idle);
			}
		}
		}, 0.5f, false);


	FTimerHandle InvincibleTimer;
	GetWorldTimerManager().SetTimer(InvincibleTimer, [this]() {
		 
		bIsInvincible = false;
		}, 0.7f, false);

	

	if (Health <= 0.0f)
	{
		SetPlayerState(EPlayerState::Dead);

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				float FadeTime = 2.0f;
				PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeTime, FLinearColor::Black, true, true);
			}
		}

		FTimerHandle ShowWidgetTimer;
		GetWorldTimerManager().SetTimer(ShowWidgetTimer, [this]() {
			if (GameOverWidgetClass)
			{
				if(GameOverWidgetClass)
					{
					GameOverWidget = CreateWidget<UUserWidget>(GetWorld(), GameOverWidgetClass);
					if (GameOverWidget)
					{
						GameOverWidget->AddToViewport();
						if (APlayerController* PC = Cast<APlayerController>(GetController()))
						{
							FInputModeUIOnly InputMode;
							InputMode.SetWidgetToFocus(GameOverWidget->TakeWidget());
							PC->SetInputMode(InputMode);
							PC->bShowMouseCursor = true;
						}
					}
				}
			}
			}, 2.0f, false);

		if (GetCharacterMovement())
		{
			GetCharacterMovement()->DisableMovement();
		}
		if (GetCapsuleComponent())
		{
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

			if (DeathMontage)
			{
				float Duration = PlayAnimMontage(DeathMontage);
				FTimerHandle DeathTimerHandle;
				GetWorld()->GetTimerManager().SetTimer(DeathTimerHandle, [this]()
					{
						//this->Destroy();
						SetPlayerState(EPlayerState::Idle);
					}, Duration, false);
			}
			
		// キャラクターが死亡したときの処理をここに追加
		UE_LOG(LogTemp, Warning, TEXT("Character has died."));
	}
	return ActualDamage;
}
void AMyProjectCharacter::BimdWidgetButtons()
{
	if (GameOverWidget)
	{

		GameOverWidget->SetIsFocusable(true);
		UButton* RestartBtn = Cast<UButton>(GameOverWidget->GetWidgetFromName(TEXT("RESTART")));
		if (RestartBtn)
		{
			UE_LOG(LogTemp, Warning, TEXT("Restart Button Clicked!!!")); 
			RestartBtn->OnClicked.AddDynamic(this, &AMyProjectCharacter::OnRestartButtonClicked);
		}
	}
}
void AMyProjectCharacter::OnRestartButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Restart"));
	FString CurrentLevelName = GetWorld()->GetMapName();
	CurrentLevelName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentLevelName));
	SetPlayerState(EPlayerState::Idle);
	Health = 100.0f;
	FInputModeGameOnly InputMode;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();

	if (PC)
	{
		PC->SetInputMode(InputMode);

		// 2. マウスカーソルを非表示にする
		PC->bShowMouseCursor = false;

	}
}
