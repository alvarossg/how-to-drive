#include "Core/HTMPlaceholderFX.h"
#include "Core/HTMVisualLibrary.h"
#include "Core/HTMPalette.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"

AHTMPlaceholderFX::AHTMPlaceholderFX()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AHTMPlaceholderFX::Init(EHTMFX InType, float InScale, float InDuration, FLinearColor InTint)
{
	Type = InType;
	Scale = InScale;
	bLoop = InDuration < 0.f;
	Duration = bLoop ? 1.f : FMath::Max(0.1f, InDuration);

	int32 Count = 6;
	FName Shape = TEXT("Sphere");
	FLinearColor Color = InTint;
	float Size = 30.f;
	FVector BaseVelocity = FVector::ZeroVector;
	float Spread = 80.f;

	switch (Type)
	{
	case EHTMFX::Smoke:       Count = 5; Color = FLinearColor(0.55f, 0.55f, 0.58f); Size = 45.f; BaseVelocity = FVector(0, 0, 80); Spread = 25.f; break;
	case EHTMFX::Fire:        Count = 6; Color = HTMPalette::SafetyOrange(); Size = 35.f; BaseVelocity = FVector(0, 0, 60); Spread = 20.f; break;
	case EHTMFX::Sparks:      Count = 8; Shape = TEXT("Cube"); Color = HTMPalette::SignalYellow(); Size = 8.f; BaseVelocity = FVector(0, 0, 250); Spread = 300.f; break;
	case EHTMFX::KOStars:     Count = 3; Color = HTMPalette::SignalYellow(); Size = 14.f; break;
	case EHTMFX::DustPoof:    Count = 6; Color = HTMPalette::FloorWarmGrey(); Size = 35.f; Spread = 160.f; break;
	case EHTMFX::WaterSplash: Count = 6; Color = HTMPalette::Water(); Size = 14.f; BaseVelocity = FVector(0, 0, 200); Spread = 180.f; break;
	case EHTMFX::PaintSplash: Count = 5; Size = 12.f; BaseVelocity = FVector(0, 0, 120); Spread = 150.f; break;
	case EHTMFX::Scrap:       Count = 5; Shape = TEXT("Cube"); Color = HTMPalette::Rust(); Size = 12.f; BaseVelocity = FVector(0, 0, 300); Spread = 220.f; break;
	case EHTMFX::Confetti:    Count = 10; Shape = TEXT("Cube"); Size = 7.f; BaseVelocity = FVector(0, 0, 420); Spread = 260.f; break;
	}

	const TArray<FLinearColor>& Confetti = HTMPalette::PaintGunColors();
	for (int32 i = 0; i < Count; ++i)
	{
		const FLinearColor BlobColor = (Type == EHTMFX::Confetti) ? Confetti[i % Confetti.Num()]
			: (Type == EHTMFX::Fire && i % 2 == 1) ? HTMPalette::SignalYellow() : Color;
		UStaticMeshComponent* Comp = UHTMVisualLibrary::AddShape(this, GetRootComponent(), Shape, FVector::ZeroVector,
			FVector(Size * Scale), FRotator::ZeroRotator, BlobColor, false);
		Comp->SetCastShadow(false);
		FBlob Blob;
		Blob.Comp = Comp;
		Blob.Velocity = (BaseVelocity + FMath::VRand() * Spread) * Scale;
		Blob.Phase = (float)i / (float)Count * UE_TWO_PI;
		Blob.BaseSize = Size * Scale;
		Blobs.Add(Blob);
		BlobComponents.Add(Comp);
	}

	if (Type == EHTMFX::Fire)
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(GetRootComponent());
		Light->SetLightColor(HTMPalette::SafetyOrange());
		Light->SetIntensity(3000.f * Scale);
		Light->SetAttenuationRadius(400.f * Scale);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	}
}

void AHTMPlaceholderFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = bLoop ? FMath::Fmod(Age, Duration) / Duration : FMath::Clamp(Age / Duration, 0.f, 1.f);

	for (FBlob& Blob : Blobs)
	{
		if (!Blob.Comp)
		{
			continue;
		}
		switch (Type)
		{
		case EHTMFX::KOStars:
		{
			const float Angle = Age * 5.f + Blob.Phase;
			Blob.Comp->SetRelativeLocation(FVector(FMath::Cos(Angle) * 30.f, FMath::Sin(Angle) * 30.f, 10.f) * Scale);
			break;
		}
		case EHTMFX::Fire:
		case EHTMFX::Smoke:
		{
			// Bolas que suben, crecen y se reinician: bucle legible.
			const float Local = FMath::Fmod(Age / Duration + Blob.Phase / UE_TWO_PI, 1.f);
			const FVector Pos = Blob.Velocity * Local + FVector(FMath::Sin(Blob.Phase + Age * 3.f) * 10.f, 0.f, 0.f);
			Blob.Comp->SetRelativeLocation(Pos);
			const float SizeT = (Type == EHTMFX::Fire) ? (1.f - Local) : (0.4f + Local);
			Blob.Comp->SetRelativeScale3D(FVector(Blob.BaseSize / 100.f * FMath::Max(0.05f, SizeT)));
			break;
		}
		default:
		{
			Blob.Velocity.Z -= 980.f * DeltaSeconds * (Type == EHTMFX::DustPoof ? 0.f : 1.f);
			Blob.Comp->AddRelativeLocation(Blob.Velocity * DeltaSeconds);
			Blob.Comp->SetRelativeScale3D(FVector(Blob.BaseSize / 100.f * FMath::Max(0.05f, 1.f - T)));
			break;
		}
		}
	}

	if (Light)
	{
		Light->SetIntensity(3000.f * Scale * (0.8f + 0.2f * FMath::Sin(Age * 23.f)));
	}

	if (!bLoop && Age >= Duration)
	{
		Destroy();
	}
}
