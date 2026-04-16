#include "MyEnemyActorComponent.h"
#include "MyCrystalMagic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MyEnemyProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blueprint/UserWidget.h"
#include "MyEnemyHealthWidget.h"
#include "MyProjectCharacter.h" // ★これを必ず冒頭に追加！
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

UMyEnemyActorComponent::UMyEnemyActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentState = EEnemyState::Idle;
	Health = 1000.0f;
	FireInterval = 5.0f;
	LastProjectileTime = -2.0f;
	MeleeRange = 1000.0f;
	DashSpeed = 1500.0f; // ★初期値
	bIsAttacking = false;

}

void UMyEnemyActorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UMyEnemyActorComponent::HandleTakeDamage);
	}
	

	if (HealthWidgetClass)
	{
		// 第一引数を GetWorld() に変更
		HealthWidget = CreateWidget<UMyEnemyHealthWidget>(GetWorld(), HealthWidgetClass);

		if (HealthWidget)
		{
		    HealthWidget->AddToViewport();
			HealthWidget->UpdateHealthPercent(Health, 1000.0f);
		}
	}

	UpdateRandomGuard();
}

void UMyEnemyActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentState)
	{
	case EEnemyState::Idle:      UpdateIdle(DeltaTime); break;
	case EEnemyState::Chasing: UpdateChasing(DeltaTime); break;
	case EEnemyState::Dashing:   UpdateDashing(DeltaTime); break;
	case EEnemyState::Attacking: UpdateAttacking(DeltaTime); break;
	case EEnemyState::Recovery:   UpdateRecovery(DeltaTime); break;
	case EEnemyState::ChronosJitter: break;
	default: break;
	}


	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) >= 1.0f && AccumulatedDamage > 0.0f)
	{
		ReleaseAccumulatedDamage();
	}
}

void UMyEnemyActorComponent::ReleaseAccumulatedDamage()
{

	SetState(EEnemyState::ChronosJitter);

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar) return;

	// 1. ダメージ適用とカウント設定
	Health = FMath::Max(Health - AccumulatedDamage, 0.0f);
	RemainingJitterCount = 2 * StoredChronosLights.Num();
	DefaultRemainingJitterCount = RemainingJitterCount;

	if (HealthWidget)
	{
		HealthWidget->UpdateHealthPercent(Health, 1000.0f);
	}

	// 2. 蓄積したエフェクトを消去
	for (AActor* Light : StoredChronosLights)
	{
		if (Light) Light->Destroy();
	}
	StoredChronosLights.Empty();

	FVector CurrentLoc = OwnerChar->GetActorLocation();
	// 地面から少し浮かせた位置を計算（例：現在の高さ + 50ユニット）
	FVector TargetLoc = CurrentLoc + FVector(0, 0, 50.0f);

	// 瞬間移動だとガクつくので、スイープ（衝突判定）ありで移動
	FHitResult Hit;
	OwnerChar->SetActorLocation(TargetLoc, true, &Hit);

	// 3. ガクガク演出開始
	if (RemainingJitterCount > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			JitterTimerHandle,
			this,
			&UMyEnemyActorComponent::ApplyJitterImpulse,
			0.1f,
			true
		);
	}

	if (EnemyStunMontage)
	{
		OwnerChar->PlayAnimMontage(EnemyStunMontage);
	}

	SetState(EEnemyState::Recovery);


	AccumulatedDamage = 0.0f;

	// 4. スタンからの復帰タイマー（ラムダ式でIdleに戻す）
	FTimerHandle StunRecoveryTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		StunRecoveryTimerHandle,
		[this]()
		{
			if (CurrentState != EEnemyState::Dead)
			{
				SetState(EEnemyState::Idle);
			}
		},
		5.2f,
		false
	);

	if (Health <= 0.0f)
	{ SetState(EEnemyState::Dead);
	if (DieMontage)
	{
		OwnerChar->PlayAnimMontage(DieMontage);
	}
	
	// 必要に応じて Destroy は少し遅らせる（演出のため）

	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			// 2.0秒後に消す例
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
				{
					OnUnitDead.Broadcast();
					GetOwner()->Destroy();
				}, 2.0f, false);
		});
	}
}

void UMyEnemyActorComponent::ApplyJitterImpulse()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Player && OwnerChar)
	{
		FVector ToPlayer = Player->GetActorLocation() - OwnerChar->GetActorLocation();
		ToPlayer.Z = 0.0f; // 上下（ピッチ）の回転を防ぐためにZを0にする

		// 2. ベクトルを回転値（Rotator）に変換
		FRotator NewRotation = ToPlayer.Rotation();

		// 3. キャラクターにセット
		OwnerChar->SetActorRotation(NewRotation);
	}

	// ガード句：オーナーがいないか、回数が終わっていたら終了
	if (!OwnerChar || RemainingJitterCount <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(JitterTimerHandle);
		return;
	}

	if (MagicActor)
	{
		MagicActor->Destroy();
		MagicActor = nullptr;
	}

	if (ChronosHitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetOwner(), ChronosHitSound, GetOwner()->GetActorLocation());
	}
	if (DefaultRemainingJitterCount == RemainingJitterCount)
	{
		OwnerChar->GetCharacterMovement()->StopMovementImmediately();
		OwnerChar->GetCharacterMovement()->Velocity = FVector::ZeroVector;

		OwnerChar->CustomTimeDilation = 0.001f;

		
		float HitStopDuration = 0.05f; // ここをいじって「重み」を調整してね
		FTimerHandle HitStopTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(HitStopTimerHandle,[this, OwnerChar]()
			{
				OwnerChar->CustomTimeDilation = 1.0f;
			}, HitStopDuration, false);

		FVector FinalDir = (OwnerChar->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal();
		OwnerChar->LaunchCharacter(FinalDir * 8000.0f + FVector(0, 0,300.0f), true, true);

		if (ChronosEndCameraShakeClass)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
			{
				PC->ClientStartCameraShake(ChronosEndCameraShakeClass);
				UE_LOG(LogTemp, Warning, TEXT("CameraShake Executed"));
			}
		}

		if (JitterExplosionComponent)
		{
			if (USkeletalMeshComponent* MeshComp = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
			{
				FName AttachSocketName = TEXT("spine_03");

				UNiagaraComponent* NSComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
					JitterExplosionComponent,
					MeshComp,
					AttachSocketName,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget,
					true
				);
			}
		}

		RemainingJitterCount--;
	}
	else
	{
		OwnerChar->GetCharacterMovement()->StopMovementImmediately();

		// 2. 慣性を完全に消去（念のため）
		OwnerChar->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		// 1. プレイヤーへの方向（0度）を計算
		FVector ToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
		ToPlayer.Z = 0.0f;

		// 2. 「少しだけ目線を逸らす」角度の決定
		// プレイヤーを正面(0度)として、右に45度(315度)か、左に45度(45度)か
		// ※時計回りが正なので、左へ45度は「-45(315)」、右へ45度は「45」
		float OffsetAngle = (FMath::RandBool()) ? 20.0f : -20.0f;

		// 3. 視線を回転させてセット
		FVector LookDir = ToPlayer.RotateAngleAxis(OffsetAngle, FVector(0, 0, 1));
		OwnerChar->SetActorRotation(LookDir.Rotation());




		// 4. 後ろに下がる（衝撃ベクトル）
		// プレイヤーから離れる方向（-ToPlayer）へ飛ばす
		FVector BackwardDir = -ToPlayer;
		FVector Impulse = (BackwardDir * JitterStrength) + FVector(0, 0, 1.0f);
		OwnerChar->LaunchCharacter(Impulse, true, true);

		// --- アニメーション再生（目線を逸らした方向と逆にのけぞる演出） ---
		if (OffsetAngle > 0.0f && Health > 0) // 右を向いた ＝ 左からの衝撃で顔が右に流れた
		{
			if (HitReactLeftMontage) OwnerChar->PlayAnimMontage(HitReactLeftMontage);
		}
		else // 左を向いた
		{
			if (HitReactRightMontage && Health > 0) OwnerChar->PlayAnimMontage(HitReactRightMontage);
		}
		if (JitterExplosionComponent)
		{
			if (USkeletalMeshComponent* MeshComp = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
			{
				FName AttachSocketName = TEXT("spine_03");

				UNiagaraComponent* NSComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
					JitterExplosionComponent,
					MeshComp,
					AttachSocketName,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget,
					true
				);
			}
		}

		RemainingJitterCount--;

		// 最後の一撃（ここは共通）
		if (RemainingJitterCount <= 0)
		{
			GetWorld()->GetTimerManager().ClearTimer(JitterTimerHandle);
			FVector FinalDir = (OwnerChar->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal();
			OwnerChar->LaunchCharacter(FinalDir * 1200.0f + FVector(0, 0, 400.0f), true, true);

			if (AttackHitSound)
			{
				UGameplayStatics::PlaySoundAtLocation(GetOwner(), AttackHitSound, GetOwner()->GetActorLocation());
			}
			ChronosDamageTime = GetWorld()->GetTimeSeconds();
		}
	}
	
	
}

void UMyEnemyActorComponent::SetState(EEnemyState NewState)
{
	if (CurrentState == NewState) return;
	if (CurrentState == EEnemyState::Attacking) bIsAttacking = false;
	CurrentState = NewState;

	switch (CurrentState)
	{
	case EEnemyState::Dashing:
		bHasLaunched = false;
		break;

	case EEnemyState::Attacking:
		bIsAttacking = true;
		ResetHitActors();
		if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
		{
			// ★ 5,500本売るための執念の向き固定
			APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
			if (Player)
			{
				FVector DirToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
				FRotator NewRot = DirToPlayer.Rotation();
				NewRot.Pitch = 0.0f;
				// 攻撃開始の瞬間に、強制的にプレイヤーを向かせる（これで反転を上書き！）
				OwnerChar->SetActorRotation(NewRot);
			}

			UAnimMontage* MontageToPlay = nullptr;

			// 2. 条件によって「中身」を入れる
			if (NextAttackType == EAttackType::SequenceAttack)
			{
				MontageToPlay = MeleeAttackMontage_L;
			}
			else if(NextAttackType == EAttackType::DashMelee || NextAttackType == EAttackType::BackStepAttack || NextAttackType == EAttackType::BehindAttack || NextAttackType == EAttackType::JumpAttack)
			{
			    MontageToPlay = MeleeAttackMontage_R;
			}
			else if (NextAttackType == EAttackType::AreaBlast)
			{
				MontageToPlay = AreaBlastMontage;
			}
			else if (NextAttackType == EAttackType::CrystalMagic)
			{
				MontageToPlay = SpawnCrystalMagicMontage;
			}
			// 3. 最後に再生する（ifの外なので、ここでも使える！）
			if (MontageToPlay && OwnerChar)
			{
				OwnerChar->PlayAnimMontage(MontageToPlay);
			}
		}
		break;

	case EEnemyState::Recovery:
		if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
		{
			OwnerChar->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
			if (Player)
			{
				FVector DirToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
				FRotator NewRot = DirToPlayer.Rotation();
				NewRot.Pitch = 0.0f;
				// 攻撃開始の瞬間に、強制的にプレイヤーを向かせる（これで反転を上書き！）
				OwnerChar->SetActorRotation(NewRot);
			}
		}


		RecoveryStartTime = GetWorld()->GetTimeSeconds();
		break;

	case EEnemyState::Stunned:
		bIsAttacking = false;
		if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
		{
			if (EnemyStunMontage) OwnerChar->PlayAnimMontage(EnemyStunMontage);
		}
		break;

	case EEnemyState::ChronosJitter:
		break;
	}
}

void UMyEnemyActorComponent::UpdateIdle(float DeltaTime)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn || !GetOwner()) return;

	float Distance = FVector::Dist(GetOwner()->GetActorLocation(), PlayerPawn->GetActorLocation());
	
	if (Distance <= ChaseRange)
	{
		SetState(EEnemyState::Chasing);
	}
}

void UMyEnemyActorComponent::UpdateChasing(float DeltaTime)
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!OwnerChar || !Player) return;

	float Distance = FVector::Dist(OwnerChar->GetActorLocation(), Player->GetActorLocation());
	FVector Dir = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();

	// ★ 物理移動(SetActorLocation)をコメントアウトし、こちらを活かす
	OwnerChar->AddMovementInput(Dir, 1.0f);

	// 向きの更新（これはそのまま）
	FRotator TargetRot = Dir.Rotation();
	TargetRot.Pitch = 0.0f;
	OwnerChar->SetActorRotation(FMath::RInterpTo(OwnerChar->GetActorRotation(), TargetRot, DeltaTime, 5.0f));

	if (Distance <= MeleeRange && CurrentState != EEnemyState::Attacking)
	{
		ExecuteAttack(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	else if (Distance > ChaseRange + 200.0f)
	{
		SetState(EEnemyState::Idle);
	}
}


void UMyEnemyActorComponent::UpdateDashing(float DeltaTime)
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetCharacterMovement()) return;

	UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement();

	if (NextAttackType == EAttackType::BackStepAttack || NextAttackType == EAttackType::BehindAttack)
	{
		// 1. 溜め開始の判定
		if (LandTime <= 0.0f)
		{
			if (MoveComp->IsFalling()) return; // 空中なら待ち

			LandTime = GetWorld()->GetTimeSeconds();
			MoveComp->Velocity = FVector::ZeroVector;
			bHasLaunched = false;
			OwnerChar->StopAnimMontage();
			
			if (!bFirstLanded)
			{
				if (BackAttackLandMontage)
				{
					OwnerChar->PlayAnimMontage(BackAttackLandMontage);
					UE_LOG(LogTemp, Warning, TEXT("着地しました！溜めを開始します。"));
				}
				bFirstLanded = true;
				
			}
			
			return;
		}

		float Elapsed = GetWorld()->GetTimeSeconds() - LandTime;

		// 2. 溜め中（1秒未満）
		if (Elapsed < 1.0f)
		{
			APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
			if (Player)
			{
				FVector ToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
				FRotator LookAtRot = ToPlayer.Rotation();
				LookAtRot.Pitch = 0.0f;
				OwnerChar->SetActorRotation(FMath::RInterpTo(OwnerChar->GetActorRotation(), LookAtRot, DeltaTime, 10.0f));
			}
			return;
		}
		if (Elapsed >= 1.0f && Elapsed < 3.0f)
		{
			

			if (!bHasLaunched)
			{
				if (MeleeAttackRunMontage) OwnerChar->PlayAnimMontage(MeleeAttackRunMontage);
				MoveComp->MaxWalkSpeed = 10000.0f;
				// ★ 執念の追加：加速度も爆上げして、一瞬で最高速にするyp！
				MoveComp->MaxAcceleration = 10000.0f;

				// 摩擦（ブレーキ）も一時的に減らすとさらにスムーズyp！
				MoveComp->BrakingDecelerationWalking = 0.0f;
				bHasLaunched = true;
			}

			APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0); // APawm を修正
			if (Player)
			{
				FVector ToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();
				OwnerChar->AddMovementInput(ToPlayer, 1.0f);

				FRotator LookAtRot = ToPlayer.Rotation();
				LookAtRot.Pitch = 0.0f;
				// SetActorRotation, GetActorRotation に修正
				OwnerChar->SetActorRotation(FMath::RInterpTo(OwnerChar->GetActorRotation(), LookAtRot, DeltaTime, 15.0f));

				float Dist = FVector::Dist(OwnerChar->GetActorLocation(), Player->GetActorLocation());
				if (Dist < 500.0f)
				{
					LandTime = -1.0f;
					MoveComp->MaxWalkSpeed = 2000.0f;
					bHasLaunched = false;
					SetState(EEnemyState::Attacking); // Attackig を修正
					return;
				}
			}
		}
		if (Elapsed >= 3.0f)
		{
			LandTime = -1.0f;

			MoveComp->MaxWalkSpeed = 2000.0f; // 元に戻す
			bHasLaunched = false;

			SetState(EEnemyState::Attacking);
		}
	}


	// --- パターンB：通常の突進（DieRoll 0） ---
	if (NextAttackType == EAttackType::DashMelee)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			// 1. 進行方向の計算
			FVector ToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();

			// 2. プレイヤーに向かって移動入力を与え続ける
			// [ Add Movement Input ] (Dir: ToPlayer) ───> (実行)
			OwnerChar->AddMovementInput(ToPlayer, 1.0f);

			// 3. 向きもプレイヤーに合わせる（旋回性能）
			FRotator LookAtRot = ToPlayer.Rotation();
			LookAtRot.Pitch = 0.0f;
			OwnerChar->SetActorRotation(FMath::RInterpTo(OwnerChar->GetActorRotation(), LookAtRot, DeltaTime, 10.0f));

			if (!bHasLaunched)
			{
				if (MeleeAttackRunMontage) OwnerChar->PlayAnimMontage(MeleeAttackRunMontage);
				bHasLaunched = true;
			}



			// 4. 距離判定：十分に近づいたら斬撃へ移行
			float Dist = FVector::Dist(OwnerChar->GetActorLocation(), Player->GetActorLocation());

			// UE_LOG で距離をデバッグすると調整しやすいです
			// UE_LOG(LogTemp, Display, TEXT("DashMelee Distance: %f"), Dist);

			if (Dist < 400.0f)
			{
				SetState(EEnemyState::Attacking);
			}
		}

		// 5. 万が一引っかかった時のための速度判定（予備）
		if (MoveComp->Velocity.Size() < 10.0f)
		{
			// 壁に詰まった場合などは攻撃を出してリセット
			SetState(EEnemyState::Attacking);
		}
	}
	// --- パターンC：連続攻撃（DieRoll ２） ---
	if (NextAttackType == EAttackType::SequenceAttack)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			// 1. 進行方向の計算
			FVector ToPlayer = (Player->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal();

			// 2. プレイヤーに向かって移動入力を与え続ける
			// [ Add Movement Input ] (Dir: ToPlayer) ───> (実行)
			OwnerChar->AddMovementInput(ToPlayer, 1.0f);

			// 3. 向きもプレイヤーに合わせる（旋回性能）
			FRotator LookAtRot = ToPlayer.Rotation();
			LookAtRot.Pitch = 0.0f;
			OwnerChar->SetActorRotation(FMath::RInterpTo(OwnerChar->GetActorRotation(), LookAtRot, DeltaTime, 10.0f));

              if (!bHasLaunched)
               {
	          if (MeleeAttackRunMontage) OwnerChar->PlayAnimMontage(MeleeAttackRunMontage);
	                bHasLaunched = true;
                }



              // 4. 距離判定：十分に近づいたら斬撃へ移行
              float Dist = FVector::Dist(OwnerChar->GetActorLocation(), Player->GetActorLocation());

// UE_LOG で距離をデバッグすると調整しやすいです
// UE_LOG(LogTemp, Display, TEXT("DashMelee Distance: %f"), Dist);

                   if (Dist < 400.0f)
                   {
	                 SetState(EEnemyState::Attacking);
                    }
		}

		// 5. 万が一引っかかった時のための速度判定（予備）
		if (MoveComp->Velocity.Size() < 10.0f)
		{
			// 壁に詰まった場合などは攻撃を出してリセット
			SetState(EEnemyState::Attacking);
		}
	}
	if (NextAttackType == EAttackType::JumpAttack)
	{
		if (MoveComp->IsFalling()) return;

		MoveComp->GravityScale = 1.0f; // 元に戻す


		if (JumpAttackTime <= 0.0f)
		{
			JumpAttackTime = GetWorld()->GetTimeSeconds();
		}
		float Elapsed = GetWorld()->GetTimeSeconds() - JumpAttackTime;
		if (Elapsed < 2.0f)
		{
			return;
		}
		else
		{
			if (!bHasLaunched && MeleeAttackMontage_R)
			{
				MoveComp->Velocity = FVector::ZeroVector;
				OwnerChar->PlayAnimMontage(MeleeAttackMontage_R);
				bHasLaunched = true;
				SetState(EEnemyState::Attacking);
			}

		}

	}
}
void UMyEnemyActorComponent::UpdateRecovery(float DeltaTime)
{
	float RecoveryDuration = 2.0f;
	if (GetWorld()->GetTimeSeconds() - RecoveryStartTime >= RecoveryDuration)
	{
		SetState(EEnemyState::Idle);
	}
}

void UMyEnemyActorComponent::HandleTakeDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
	ApplyDamage(Damage);
}

void UMyEnemyActorComponent::ExecuteAttack(FVector MuzzleLocation, FRotator MuzzleRotation)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float Elapsed = CurrentTime - ChronosDamageTime;
	AActor* Owner = GetOwner();
	ACharacter* OwnerChar = Cast<ACharacter>(Owner);
	if (!OwnerChar) return;
	UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement();
	if (!MoveComp) return;

	if (Elapsed < 3.0f || (OwnerChar && MoveComp->IsFalling()))
	{
		return;
	}

	// すでに攻撃中なら無視
	if (CurrentState == EEnemyState::Dashing || CurrentState == EEnemyState::Attacking) return;

	if (AttackRoarSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetOwner(), AttackRoarSound, GetOwner()->GetActorLocation());
	}



	// ★ この1行を追加してPlayerを取得する！
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;
	// ★ ここで「あだ名」を定義！
	FVector PlayerLoc = Player->GetActorLocation();
	FVector EnemyLoc = GetOwner()->GetActorLocation();
	FVector ToPlayer = (PlayerLoc - EnemyLoc).GetSafeNormal();

	// --- 執念のランダム分岐：どの攻撃を出すか決める ---
	int32 DieRoll = FMath::RandRange(0, 5); // 0〜2の乱数
	if ((PlayerLoc - EnemyLoc).Size() > 3000.0f)
	{
		DieRoll =  6;
	}

	if (DieRoll == 0)
	{
		// パターン0：突進攻撃
		NextAttackType = EAttackType::DashMelee;

		if (OwnerChar)
		{
			FVector LaunchVel = ToPlayer * 2000.0f;
			OwnerChar->LaunchCharacter(LaunchVel, true, true);
		}
		
		

		SetState(EEnemyState::Dashing);
	}
	else if (DieRoll == 1)
	{
		// パターン1：バックステップアタック
		NextAttackType = EAttackType::BackStepAttack;

		DashTargetLocation = EnemyLoc - (ToPlayer * 1000.0f);
		DashSpeed = 800.0f;
		if(OwnerChar)
		{
			OwnerChar->GetCharacterMovement()->bOrientRotationToMovement = false;

			if (BackAttackMontage)
			{
				OwnerChar->PlayAnimMontage(BackAttackMontage);
			}
			FVector LaunchVelocity = (-ToPlayer * 1000.0f) + FVector(0, 0, 700.0f);
		    OwnerChar->LaunchCharacter(LaunchVelocity, true, true);

			FRotator LookAtRot = ToPlayer.Rotation();
			LookAtRot.Pitch = 0.0f;
			OwnerChar->SetActorRotation(LookAtRot);
		}

		
		SetState(EEnemyState::Dashing); 
	}
	else if (DieRoll == 2)
	{
		
		NextAttackType = EAttackType::SequenceAttack;

		if (OwnerChar)
		{
			FVector LaunchVel = ToPlayer * 2000.0f;
			OwnerChar->LaunchCharacter(LaunchVel, true, true);
		}

		SetState(EEnemyState::Dashing);
	}
	else if (DieRoll == 3)
	{

		NextAttackType = EAttackType::CrystalMagic;
		SetState(EEnemyState::Attacking);

		FTimerHandle MagicHandle;
		GetWorld()->GetTimerManager().SetTimer(
			MagicHandle,
			this,
			&UMyEnemyActorComponent::ExecuteCrystalMagic,
			1.2f,
			false
		);
	}
	else if (DieRoll == 4)
	{

		NextAttackType = EAttackType::BehindAttack;
		
		
		
		if (Player && Owner)
		{
			FVector A = Owner->GetActorLocation();
			FVector B = Player->GetActorLocation();

			FVector Dir = B - A;
			FVector PointC = B + (Dir * 2.0f);

			float JumpTime = 2.0f;
			FVector HorizontalVel = (PointC - A) / JumpTime;

			float VerticalVel = 800.0f;

			FVector LaunchVel = HorizontalVel + FVector(0, 0, VerticalVel);
			if (OwnerChar)
			{
				OwnerChar->LaunchCharacter(LaunchVel, true, true);
				if (BackAttackMontage)
				{
					OwnerChar->PlayAnimMontage(BackAttackMontage);
				}
			}

			LandTime = 0.0f;      // ★これを忘れると前回の時間が残る
			bFirstLanded = false; // ★これを忘れると2回目からモーションが出ない
			SetState(EEnemyState::Dashing);

		}
	}
	else if (DieRoll == 5)
	{	
			// パターン2：範囲攻撃
			NextAttackType = EAttackType::AreaBlast;
			SetState(EEnemyState::Attacking); // 直接アタックスベートへ
			FTimerHandle PreWaitHandle;
			GetWorld()->GetTimerManager().SetTimer(
				PreWaitHandle,
				this,
				&UMyEnemyActorComponent::SpawnAreaBlast, // 新しく作る関数
				1.2f,
				false
			);

		}

	else
	{
		NextAttackType = EAttackType::JumpAttack;

		if (Player && Owner)
		{

			MoveComp->GravityScale = 0.5f; // 重力を半分にする
			bJumpAttacked = true;
			FVector A = Owner->GetActorLocation();
			FVector B = Player->GetActorLocation();

			FVector Dir = B - A;

			float CurrentDistance = Dir.Size();
			float TargetDistance = FMath::Max(0.0f, CurrentDistance - 300.0f);

			FVector AdjustDir = Dir.GetSafeNormal() * TargetDistance;

			float JumpTime = 2.0f;
			FVector HorizontalVel = (AdjustDir) / JumpTime;

			float VerticalVel = 500.0f;

			FVector LaunchVel = HorizontalVel + FVector(0, 0, VerticalVel);
			if (OwnerChar)
			{
				OwnerChar->LaunchCharacter(LaunchVel, true, true);
				if (LauchPad)
				{
					OwnerChar->PlayAnimMontage(LauchPad, 1.0f, FName("Start"));
					UE_LOG(LogTemp, Warning, TEXT("ジャンプ攻撃を実行！"));
				}
			}

			JumpAttackTime = GetWorld()->GetTimeSeconds();
			SetState(EEnemyState::Dashing);
		}
	}
}
void UMyEnemyActorComponent::SpawnAreaBlast()
{
	FireRadialProjectiles(0.0f);

	float HalfStep = (360.0f / BlastProjectileCount) * 0.5f;

	FTimerHandle SecondWaveHandle;
	GetWorld()->GetTimerManager().SetTimer(
		SecondWaveHandle,
		this,
		&UMyEnemyActorComponent::SpawnSecondWave,
		1.0f,
		false
	);

	FTimerHandle ThirdWaveHandle;
	GetWorld()->GetTimerManager().SetTimer(
		ThirdWaveHandle,
		this,
		&UMyEnemyActorComponent::SpawnThirdWave,
		2.0f,
		false
	);
}

void UMyEnemyActorComponent::PushPlayerAway()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		FVector PushDir = PlayerPawn->GetActorLocation() - GetOwner()->GetActorLocation();
		PushDir.Z = 200;
		PushDir.Normalize();

		if (ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
		{
			PlayerCharacter->LaunchCharacter(PushDir * 2000.0f, true, false);
		}

	}
}

void UMyEnemyActorComponent::SpawnSecondWave()
{
	float AngleOffset = (360.0f / BlastProjectileCount) * 0.5f;
	FireRadialProjectiles(AngleOffset);
}

void UMyEnemyActorComponent::SpawnThirdWave()
{
	FireRadialProjectiles(0.0f); // ここで 0.0f を渡す
}

void UMyEnemyActorComponent::FireRadialProjectiles(float AngleOffset)
{
	AActor* Owner = GetOwner();
	if (!Owner || !ProjectileClass) return;

	FVector SpawnLocation = Owner->GetActorLocation();

	float AngleStep  = 360.0f / BlastProjectileCount;

	for (int32 i = 0; i < BlastProjectileCount; i++)
	{
		float CurrentAngle = (i * AngleStep) + AngleOffset;

		FRotator SpawnRotation = FRotator(0.0f, CurrentAngle, 0.0f);
		FVector LaunchDirection = SpawnRotation.Vector();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.Instigator = Owner->GetInstigator();

		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* SpawnedTama = GetWorld()->SpawnActor<AActor>(
			ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams
		);
		if (SpawnedTama)
		{
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(SpawnedTama->GetRootComponent()))
			{
				RootPrim->IgnoreActorWhenMoving(Owner, true);
			}
			// 2. 弾丸コンポーネントを探して速度を設定
			UProjectileMovementComponent* PMC = SpawnedTama->FindComponentByClass<UProjectileMovementComponent>();
			if (PMC)
			{
				PMC->Velocity = LaunchDirection * 1000.0f;
			}
		}
	}
}


void UMyEnemyActorComponent::UpdateAttacking(float DeltaTime)
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar) return;

	// --- 【修正ポイント】判定を出すかどうかのチェック ---
	bool bIsJumping = (NextAttackType == EAttackType::JumpAttack && OwnerChar->GetCharacterMovement()->IsFalling());

	if (NextAttackType != EAttackType::AreaBlast &&  NextAttackType != EAttackType::CrystalMagic && !bIsJumping)
	{
		TickMeleeDetection();
	}
}
void UMyEnemyActorComponent::ResetHitActors() { HitActors.Empty(); }
void UMyEnemyActorComponent::OnAttackAnimationFinished() { SetState(EEnemyState::Recovery); }
void UMyEnemyActorComponent::DamageHealth(float DamageAmount) { ApplyDamage(DamageAmount); }

void UMyEnemyActorComponent::TickMeleeDetection()
{
	if (!bIsAttacking || CurrentState == EEnemyState::Dead) return;

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->GetMesh()) return;

	USkeletalMeshComponent* Mesh = OwnerChar->GetMesh();

	// --- 判定ロジックの本体 (共通処理) ---
	auto PerformTrace = [&](FName BaseSocket, FName TipSocket)
		{
			if (Mesh->DoesSocketExist(BaseSocket) && Mesh->DoesSocketExist(TipSocket))
			{
				FVector StartPos = Mesh->GetSocketLocation(BaseSocket);
				FVector EndPos = Mesh->GetSocketLocation(TipSocket);

				float CapsuleRadius = 200.0f;
				float CapsuleHalfHeight = FVector::Dist(StartPos, EndPos) * 0.5f;

				FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
				FVector CapsuleVec = (EndPos - StartPos).GetSafeNormal();

				// 0ベクトルのエラー回避
				if (CapsuleVec.IsNearlyZero()) return;

				FRotator CapsuleRot = FRotationMatrix::MakeFromZ(CapsuleVec).Rotator();
				FVector CapsuleCenter = (StartPos + EndPos) * 0.5f;

				FCollisionQueryParams Params;
				Params.AddIgnoredActor(OwnerChar);

				// デバッグ表示（右手は黄色、左手は水色とかにすると分かりやすい！）
				//FColor DebugColor = (BaseSocket.ToString().Contains("Left")) ? FColor::Yellow : FColor::Yellow;
				//DrawDebugCapsule(GetWorld(), CapsuleCenter, CapsuleHalfHeight, CapsuleRadius, CapsuleRot.Quaternion(), DebugColor, false, 0.1f);

				TArray<FHitResult> HitResults;
				bool bHit = GetWorld()->SweepMultiByChannel(
					HitResults,
					CapsuleCenter,
					CapsuleCenter + FVector(0, 0, 0.1f),
					CapsuleRot.Quaternion(),
					ECC_Pawn,
					Capsule,
					Params
				);

				if (bHit)
				{
					for (const FHitResult& Hit : HitResults)
					{
						AActor* HitActor = Hit.GetActor();
						if (HitActor && !HitActors.Contains(HitActor))
						{
							if (APawn* HitPawn = Cast<APawn>(HitActor))
							{
								if (HitPawn->IsPlayerControlled())
								{
									HitActors.Add(HitActor);
									UGameplayStatics::ApplyDamage(HitActor, 10.0f, OwnerChar->GetInstigatorController(), OwnerChar, UDamageType::StaticClass());
									UE_LOG(LogTemp, Log, TEXT("Sword Hit! (Socket: %s)"), *BaseSocket.ToString());
								}
							}
						}
					}
				}
			}
		};

	// --- 実際の判定実行 ---

	// 1. 右手の剣（常に判定）
	PerformTrace(FName("SwordStart"), FName("SwordEnd"));

	// 2. 左手の剣（SequenceAttackの時だけ追加）
	if (NextAttackType == EAttackType::SequenceAttack)
	{
		// ソケット名はエディタ側で作成した名前に合わせてください
		PerformTrace(FName("LeftSwordStart"), FName("LeftSwordEnd"));
	}
}
void UMyEnemyActorComponent::ExecuteCrystalMagic()
{
	if (!CrystalMagicClass) return;

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!OwnerChar || !Player)return;

	FVector Forward = OwnerChar->GetActorForwardVector();
	FVector SpawnLocation = OwnerChar->GetActorLocation() + (Forward * 150.0f) + FVector(0, 0, 100.0f);

	FVector ToPlayer = (Player->GetActorLocation() - SpawnLocation).GetSafeNormal();
	FRotator SpawnRotation = ToPlayer.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = OwnerChar;

	 MagicActor = GetWorld()->SpawnActor<AMyCrystalMagic>(
		CrystalMagicClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);
}

EDirectionalSector UMyEnemyActorComponent::GetPlayerSector(AActor* PlayerActor)
{
	if (!PlayerActor || !GetOwner()) return EDirectionalSector::None;

	// [ Actor Location ] ────> [ Inverse Transform ] ────> [ Local Relative Position ]
	FVector RelativePos = GetOwner()->GetActorTransform().InverseTransformPosition(PlayerActor->GetActorLocation());

	float AbsY = FMath::Abs(RelativePos.Y);
	float AbsZ = FMath::Abs(RelativePos.Z);

	// Z軸(上下)の差がY軸(左右)より大きい場合は上下セクター
	if (AbsZ > AbsY)
	{
		return (RelativePos.Z > 0.0f) ? EDirectionalSector::Up : EDirectionalSector::Down;
	}
	else
	{
		return (RelativePos.Y > 0.0f) ? EDirectionalSector::Right : EDirectionalSector::Left;
	}
}

void UMyEnemyActorComponent::UpdateRandomGuard()
{
	// 1. [ 執念の初期化 ] 要素数を 4 (0, 1, 2, 3) に固定し、一旦全部ガードなし(false)にする
	if (bIsDirectionGuarded.Num() != 4)
	{
		bIsDirectionGuarded.Init(false, 4);
	}

	// 全リセット（これを忘れると前回のガードが残る！）
	for (int32 j = 0; j < 4; j++) bIsDirectionGuarded[j] = false;

	// 2. [ 抽選 ] 0〜3 のうち、どこか 1箇所だけを空ける(falseのままにする)
	int32 OpenSlot = FMath::RandRange(0, 3);

	for (int32 i = 0; i < 4; i++)
	{
		// OpenSlot 以外をガード中(true)にする
		if (i != OpenSlot)
		{
			bIsDirectionGuarded[i] = true;
		}
		if (GuardShields.IsValidIndex(i) && GuardShields[i])
		{
			AActor* ShieldActor = GuardShields[i];
			bool bShouldShow = bIsDirectionGuarded[i];

			// 1. 見た目の切り替え（Actor用。bNewHiddenなので !bShouldShow）
			ShieldActor->SetActorHiddenInGame(!bShouldShow);

			// 2. 衝突判定の切り替え
			// もしシールドがスタティックメッシュ等を持っているなら
			ShieldActor->SetActorEnableCollision(bShouldShow);

			// 3. Niagaraなどのエフェクトが含まれている場合
			// 子コンポーネントも含めて全停止/再生をかける執念の処理
			if (bShouldShow) {
				ShieldActor->SetActorTickEnabled(true);
			}
			else {
				ShieldActor->SetActorTickEnabled(false);
			}
		}
		
	}
}

float UMyEnemyActorComponent::ApplyDamage(float DamageAmount)
{
	if (DamageAmount <= 0.0f || CurrentState == EEnemyState::Dead) return Health;

	// 1. 【共通準備】プレイヤーを取得し、自作クラスにキャストする
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	AMyProjectCharacter* PlayerChar = Cast<AMyProjectCharacter>(PlayerPawn);
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

	// 安全策：プレイヤーが見つからない場合はそのままダメージを通す
	if (!PlayerChar || !GetOwner()) return Health;

	
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) > 0.5f)
	{
		// --- 背後判定 ---
		FVector EnemyForward = GetOwner()->GetActorForwardVector();
		FVector ToPlayer = (PlayerChar->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
		float Dot = FVector::DotProduct(EnemyForward, ToPlayer);

		bool bIsBehind = (Dot < 0.0f);

		
			// --- 振り方向と盾のガチンコ比較 ---
			EAttackDirection PDir = PlayerChar->CurrentAttackDirection;
			bool bIsBlocked = false;

			// [ プレイヤーの振り ] ────> [ 敵のガード配列 ]
			if (PDir == EAttackDirection::Vertical_Up && bIsDirectionGuarded.IsValidIndex(0) && bIsDirectionGuarded[0]) bIsBlocked = true;
			if (PDir == EAttackDirection::Vertical_Down && bIsDirectionGuarded.IsValidIndex(1) && bIsDirectionGuarded[1]) bIsBlocked = true;
			if (PDir == EAttackDirection::Horizontal_L && bIsDirectionGuarded.IsValidIndex(2) && bIsDirectionGuarded[2]) bIsBlocked = true;
			if (PDir == EAttackDirection::Horizontal_R && bIsDirectionGuarded.IsValidIndex(3) && bIsDirectionGuarded[3]) bIsBlocked = true;

			if (bIsBlocked)
			{
				PlayParryReaction();
				return Health; // 弾かれたのでここで終了！
			}

			UpdateRandomGuard();
			// 4. 通常ダメージ適用
			Health = FMath::Max(Health - DamageAmount, 0.0f);
		
	}
	

	// --- ここから下は「ヒット確定（背後 or ガードを抜けた）」時のロジック ---

	// 3. クロノス（時間停止）中の蓄積
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) < 0.01f)
	{
		// ダメージを蓄積（あとで Release で一気に引く用）
		AccumulatedDamage += DamageAmount;

		if (ChronosLightClass)
		{
			// プレイヤーアクターを取得
			

			if (PlayerChar && PlayerChar->GetMesh())
			{
				USkeletalMeshComponent* PlayerMesh = PlayerChar->GetMesh();
				FName TipSocket = FName("End"); // プレイヤーの剣にあるソケット名

				if (PlayerMesh->DoesSocketExist(TipSocket))
				{
					// [ プレイヤーの剣の現在位置 ] ────> [ SpawnLocation ]
					FVector SpawnLoc = PlayerMesh->GetSocketLocation(TipSocket);
					FActorSpawnParameters SpawnParams;

					// 3. 光を生成（親子付けせず、その場に置いてくるだけ）
					AActor* NewLight = GetWorld()->SpawnActor<AActor>(ChronosLightClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

					if (NewLight)
					{
						// 後で消すために配列には入れておく（これは敵のリストへ）
						StoredChronosLights.Add(NewLight);

						UE_LOG(LogTemp, Warning, TEXT("Light Spawned at Player's Sword! Total: %d"), StoredChronosLights.Num());
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Socket 'End' not found on Player Mesh!"));
				}
			}
		}
		// ここではまだ Health は減らさない
		return 0.0f;
	}

	

	// UI更新・演出
	if (HealthWidget)
	{
		HealthWidget->UpdateHealthPercent(Health, 1000.0f);
		HealthWidget->PlayShakeAnimation(DamageAmount);
	}

	// 死亡判定
	if (Health <= 0.0f)
	{
		SetState(EEnemyState::Dead);
		if (DieMontage)
		{
			OwnerChar->PlayAnimMontage(DieMontage);
			for (AActor* Shield : GuardShields)
			{
				if (IsValid(Shield))
				{
					Shield->Destroy();
				}
			}
			GuardShields.Empty();
		}
		

		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				// 2.0秒後に消す例
				FTimerHandle TimerHandle;
				GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
					{
						
						GetOwner()->Destroy();
						OnUnitDead.Broadcast();
						// 必要に応じて Destroy は少し遅らせる（演出のため）

						
					}, 2.0f, false);
			});
	}
	else if (CurrentState == EEnemyState::Idle || CurrentState == EEnemyState::Stunned)
	{
		// 叩かれたらスタン状態へ
		SetState(EEnemyState::Stunned);
	}

	return Health;
}

void UMyEnemyActorComponent::PlayParryReaction()
{
	AMyProjectCharacter* PlayerChar = Cast<AMyProjectCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	if (PlayerChar)
	{
		// 敵の現在地を ImpactPoint として渡す
		PlayerChar->OnParried(GetOwner()->GetActorLocation());

		// ヒットストップなどの敵側演出
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.05f);
		// ... (以下略)
		FTimerHandle RestoreTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			RestoreTimerHandle,
			[this]()
			{
				// [ 0.05f ] ────> [ 1.0f ] (通常速度に復帰)
				UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
			},
			0.0025f,
			false
		);
	}
}
float UMyEnemyActorComponent::GetCurrentHealth()
{
	return Health;
}

FVector UMyEnemyActorComponent::GetFocusLocation() const
{
	AActor* Owner = GetOwner();

	if (!Owner) return FVector::ZeroVector;

	if (ACharacter* EnemyChar = Cast<ACharacter>(Owner))
	{
		USkeletalMeshComponent* Mesh = EnemyChar->GetMesh();

		if (Mesh && Mesh->DoesSocketExist(TEXT("SwordStart")))
		{
			UE_LOG(LogTemp, Warning, TEXT("ここにメッセージを書くyp!"));
			return Mesh->GetSocketLocation("SwordStart");
		}
	}

	 //fallback（これ超重要）
	return Owner->GetActorLocation();
}

// TickMeleeDetection, FireProjectile等は提供された通りに実装してください（長大になるため省略しますが、内容は維持されます）