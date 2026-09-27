#pragma once

#include "CoreMinimal.h"

/**
 * Paleta oficial (docs/ART_DIRECTION.md §5). Colores en sRGB convertidos a lineal.
 * Los colores fuertes se reservan para lo interactivo.
 */
namespace HTMPalette
{
	inline FLinearColor FromHex(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	inline FLinearColor WallMint()     { return FromHex(0x8F, 0xC1, 0xA9); }
	inline FLinearColor FloorWarmGrey(){ return FromHex(0xB8, 0xB0, 0xA4); }
	inline FLinearColor SafetyOrange() { return FromHex(0xFF, 0x8A, 0x3D); }
	inline FLinearColor SignalYellow() { return FromHex(0xFF, 0xD2, 0x3F); }
	inline FLinearColor ToolRed()      { return FromHex(0xE5, 0x48, 0x4D); }
	inline FLinearColor ToolBlue()     { return FromHex(0x3E, 0x7C, 0xB1); }
	inline FLinearColor Rust()         { return FromHex(0xB5, 0x65, 0x1D); }
	inline FLinearColor CarSky()       { return FromHex(0x9E, 0xD8, 0xF0); }
	inline FLinearColor CarCream()     { return FromHex(0xF6, 0xE7, 0xC1); }
	inline FLinearColor CarSalmon()    { return FromHex(0xF4, 0xA0, 0x8C); }
	inline FLinearColor CarLime()      { return FromHex(0xB9, 0xE0, 0x6A); }
	inline FLinearColor Skin()         { return FromHex(0x2F, 0x2A, 0x38); }
	inline FLinearColor White()        { return FromHex(0xFA, 0xF7, 0xF0); }
	inline FLinearColor Asphalt()      { return FromHex(0x5E, 0x5A, 0x57); }
	inline FLinearColor Dirt()         { return FromHex(0xA0, 0x7A, 0x52); }
	inline FLinearColor Grass()        { return FromHex(0x9B, 0xC4, 0x6B); }
	inline FLinearColor Water()        { return FromHex(0x6F, 0xB8, 0xD9); }
	inline FLinearColor Chrome()       { return FromHex(0xD9, 0xDD, 0xE2); }
	inline FLinearColor Rubber()       { return FromHex(0x33, 0x31, 0x36); }
	inline FLinearColor Glass()        { return FromHex(0xC8, 0xEE, 0xF5); }

	/** Colores de coche de serie. */
	inline const TArray<FLinearColor>& CarPastels()
	{
		static const TArray<FLinearColor> Colors = { CarSky(), CarCream(), CarSalmon(), CarLime() };
		return Colors;
	}

	/** Colores de la pistola de pintura (vivos: cumplen "color vivo"). */
	inline const TArray<FLinearColor>& PaintGunColors()
	{
		static const TArray<FLinearColor> Colors = {
			ToolRed(), SafetyOrange(), SignalYellow(), FromHex(0x3F, 0xC4, 0x6A),
			ToolBlue(), FromHex(0x9B, 0x5C, 0xE0), FromHex(0xFF, 0x6F, 0xB5), White()
		};
		return Colors;
	}

	/** Colores de pared del taller disponibles para personalizar. */
	inline const TArray<FLinearColor>& WallColors()
	{
		static const TArray<FLinearColor> Colors = {
			WallMint(), CarSky(), CarCream(), CarSalmon(), FromHex(0xD7, 0xC6, 0xE8), FromHex(0xF2, 0xD4, 0x8A)
		};
		return Colors;
	}
}
