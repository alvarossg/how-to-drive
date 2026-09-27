#include "Parts/SurfaceLogic.h"
#include "Core/HTMTuningData.h"

bool HTMSurface::Apply(FPartSurfaceState& S, ESurfaceTreatment Treatment, float Amount, const FLinearColor& Color, bool bPaintable)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	const bool bWasFresh = S.FreshPaint > 0.05f && !S.bPaintRuined;
	bool bRuins = false;

	switch (Treatment)
	{
	case ESurfaceTreatment::Paint:
		if (!bPaintable)
		{
			return false;
		}
		if (!S.PaintColor.Equals(Color, 0.05f))
		{
			// Color nuevo: la capa anterior queda debajo y se va cubriendo.
			S.BaseColor = S.GetVisibleColor();
			S.PaintColor = Color;
			S.PaintAmount = 0.f;
		}
		S.PaintAmount = FMath::Min(1.f, S.PaintAmount + Amount);
		S.FreshPaint = 1.f;
		S.bPaintRuined = false;
		S.Dirt = FMath::Max(0.f, S.Dirt - Amount);
		return false;

	case ESurfaceTreatment::Water:
		S.Wetness = 1.f;
		S.Dirt = FMath::Max(0.f, S.Dirt - Amount * 0.5f);
		bRuins = true;
		break;

	case ESurfaceTreatment::Sponge:
		if (S.Wetness > 0.2f)
		{
			S.Dirt = FMath::Max(0.f, S.Dirt - Amount);
		}
		bRuins = true;
		break;

	case ESurfaceTreatment::Sand:
		S.Rust = FMath::Max(0.f, S.Rust - Amount);
		S.PaintAmount = FMath::Max(0.f, S.PaintAmount - Amount * 0.5f);
		break;

	case ESurfaceTreatment::Dirt:
		S.Dirt = FMath::Min(1.f, S.Dirt + Amount * 0.3f);
		bRuins = true;
		break;

	case ESurfaceTreatment::BoothDry:
		S.FreshPaint = FMath::Max(0.f, S.FreshPaint - Amount * (T.PaintBoothDryMult / FMath::Max(1.f, T.PaintDryTime)));
		return false;
	}

	if (bRuins && bWasFresh)
	{
		S.bPaintRuined = true;
		return true;
	}
	return false;
}

bool HTMSurface::TickDrying(FPartSurfaceState& S, float DeltaSeconds)
{
	const UHTMTuningData& T = UHTMTuningData::Get();
	bool bChanged = false;
	if (S.Wetness > 0.f)
	{
		S.Wetness = FMath::Max(0.f, S.Wetness - DeltaSeconds / FMath::Max(1.f, T.WetDryTime));
		bChanged = true;
	}
	if (S.FreshPaint > 0.f)
	{
		S.FreshPaint = FMath::Max(0.f, S.FreshPaint - DeltaSeconds / FMath::Max(1.f, T.PaintDryTime));
		bChanged = true;
	}
	return bChanged;
}
