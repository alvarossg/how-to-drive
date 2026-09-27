# Lista de assets y placeholders

Todo lo que hoy se ve en el juego es **placeholder**: primitivas de `/Engine/BasicShapes` (Cube, Cylinder, Sphere, Cone) con
colores planos de la paleta oficial (`HTMPalette.h`, `docs/ART_DIRECTION.md` §5). Esta lista es el encargo para el artista:
qué sustituir, con qué nombre, presupuesto, pivote, sockets y colisión.

**Convenciones (ART_DIRECTION §3 y §14):** Blender → FBX, centímetros, Z arriba, X adelante. Colisión simple `UCX_`.
Material: instancias de `M_Master` (no texturas de color: el color es un parámetro). Sockets `SOCKET_Grip` (donde se coge),
`SOCKET_Bolt_01..N` (tornillos, en el orden en que se aprietan). Escala de referencia: el mecánico mide **120 cm** con la
cabeza ≈ 1/3 de la altura.

Leyenda de estado: ⬜ placeholder · 🟨 en curso · ✅ arte final integrado.

> Esta página se genera en parte desde `Content/Data/*.json`. Si añades piezas al JSON, añade aquí su fila.

## 1. Personaje

| Asset | Estado | Placeholder actual | Especificación final |
|---|---|---|---|
| `SK_Mechanic` | ⬜ | Cápsula blanca (cuerpo) + esfera gris-tinta (cabeza 1/3) + manoplas y botas esféricas, 120 cm (`AMechanicCharacter::BuildPlaceholderBody`) | 3.000–6.000 tris. Huesos mínimos: `root, pelvis, spine_01, spine_02, neck_01, head, clavicle_l/r, upperarm_l/r, lowerarm_l/r, hand_l/r, thigh_l/r, calf_l/r, foot_l/r`. Sockets: `SOCKET_Hand_R`, `SOCKET_Hand_L`, `SOCKET_CarryTwoHands` (delante del pecho), `SOCKET_Hat`. Hueco de material **`Face`** (texturas `EyesTex`, `BrowsTex`, `MouthTex`). Pivote en los pies. |
| `PA_Mechanic` | ⬜ | Muelle procedural sobre el placeholder (`UActiveRagdollComponent`) | Physics Asset con ≤ 14 cápsulas; límites angulares suaves; cabeza pesada. |
| `ABP_Mechanic` | ⬜ | — | Padre `UMechanicAnimInstance`. Locomoción de pie/agachado/tumbado, cargar a una/dos manos, tambaleo aditivo (`Wobble`, `LeanSide`), sentado conduciendo, KO. |
| `DA_FaceExpressions` + texturas `T_Face_*` | ⬜ | Ojos, cejas y boca de primitivas que cambian de pose (`UMechanicExpressionComponent`) | 6 expresiones: Happy, Effort, Scared, KO, Angry, Surprised. 256×256, fondo transparente, trazo grueso. Cejas = 80 % de la expresión. |
| Animaciones `A_Mechanic_*` | ⬜ | Sin animación (el placeholder se inclina con el muelle) | Idle con personalidad, andar, correr, agacharse, tumbarse/arrastrarse, cargar (esfuerzo), lanzar, apretar tornillo, bombear gato, empujar, levantarse del KO, 3 emotes (celebrar, empujar, enfado), grito "¡EH!". Exageradas, con anticipación y rebote. |

### Cosméticos

| Id | Hueco | Forma placeholder | Final |
|---|---|---|---|
| `Hat_Cap_Blue` | Hat | Cap | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Hat_None` | Hat | None | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Hat_Cap_Red` | Hat | Cap | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Hat_Beanie_Mint` | Hat | Beanie | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Hat_Helmet_Yellow` | Hat | Helmet | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Hat_Goggles` | Hat | Goggles | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Outfit_Apron_Orange` | Outfit | Apron | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Outfit_Overalls_Blue` | Outfit | Overalls | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Outfit_Overalls_Scorched` | Outfit | Overalls | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Outfit_Hoodie_Salmon` | Outfit | Hoodie | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Outfit_Hoodie_Sky` | Outfit | Hoodie | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Gloves_None` | Gloves | None | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Gloves_Mitts_Yellow` | Gloves | Mitts | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `Gloves_Mitts_Red` | Gloves | Mitts | Mesh sobre el mismo esqueleto (`SK_` o `SM_` al socket) |
| `ToolColor_Red` | ToolColor | None | Solo color |
| `ToolColor_Blue` | ToolColor | None | Solo color |
| `ToolColor_Lime` | ToolColor | None | Solo color |
| `ToolColor_Sky` | ToolColor | None | Solo color |
| `ToolColor_Chrome` | ToolColor | None | Solo color |

## 2. Piezas de coche (`DT_Parts`)

Cada pieza es un actor físico independiente. El `SM_` se asigna en la columna `Mesh` de su fila; mientras esté vacía se usa
`PlaceholderShape`/`PlaceholderSize`/`PlaceholderColor`. Las piezas `bUseCarColor` toman el color de carrocería.

| PartId | Categoría | Placeholder (forma, tamaño cm) | Masa kg | Tornillos | Asset final | Tris | Pivote |
|---|---|---|---|---|---|---|---|
| `Engine_I3` | Engine | Cube 70×60×50 | 95 | 4 | `SM_Engine_I3` | 600–1.500 | Centro de la base de apoyo; X hacia el frontal del coche. |
| `Engine_V6` | Engine | Cube 85×70×55 | 150 | 4 | `SM_Engine_V6` | 600–1.500 | Centro de la base de apoyo; X hacia el frontal del coche. |
| `Engine_V8_Huge` | Engine | Cube 110×80×70 | 230 | 6 | `SM_Engine_V8_Huge` | 600–1.500 | Centro de la base de apoyo; X hacia el frontal del coche. |
| `Engine_Electric` | Engine | Cylinder 55×55×60 | 70 | 4 | `SM_Engine_Electric` | 600–1.500 | Centro de la base de apoyo; X hacia el frontal del coche. |
| `Exhaust_Standard` | Exhaust | Cube 90×12×12 | 9 | 2 | `SM_Exhaust_Standard` | 150–500 | Punto de unión al motor/chasis; X hacia atrás a lo largo del tubo. |
| `Exhaust_Sport` | Exhaust | Cube 100×16×16 | 11 | 2 | `SM_Exhaust_Sport` | 150–500 | Punto de unión al motor/chasis; X hacia atrás a lo largo del tubo. |
| `Exhaust_GiantPipe` | Exhaust | Cube 140×40×40 | 26 | 3 | `SM_Exhaust_GiantPipe` | 150–500 | Punto de unión al motor/chasis; X hacia atrás a lo largo del tubo. |
| `Wheel_Standard` | Wheel | Cylinder 60×60×22 | 12 | 4 | `SM_Wheel_Standard` | 300–800 | Centro del buje. **Eje de giro = Z local** (el hueco lleva Roll 90). |
| `Wheel_Flat` | Wheel | Cylinder 48×60×22 | 11 | 4 | `SM_Wheel_Flat` | 300–800 | Centro del buje. **Eje de giro = Z local** (el hueco lleva Roll 90). |
| `Wheel_Offroad` | Wheel | Cylinder 72×72×30 | 18 | 4 | `SM_Wheel_Offroad` | 300–800 | Centro del buje. **Eje de giro = Z local** (el hueco lleva Roll 90). |
| `Wheel_Slick` | Wheel | Cylinder 60×60×28 | 10 | 4 | `SM_Wheel_Slick` | 300–800 | Centro del buje. **Eje de giro = Z local** (el hueco lleva Roll 90). |
| `Wheel_Giant` | Wheel | Cylinder 96×96×40 | 38 | 5 | `SM_Wheel_Giant` | 300–800 | Centro del buje. **Eje de giro = Z local** (el hueco lleva Roll 90). |
| `Wheel_Barrow` | Wheel | Cylinder 32×32×10 | 3 | 2 | `SM_Wheel_Barrow` | 300–800 | Centro del buje. **Eje de giro = Z local** (el hueco lleva Roll 90). |
| `Suspension_Standard` | Suspension | Cylinder 14×14×30 | 9 | 2 | `SM_Suspension_Standard` | 150–400 | Centro del muelle; Z = eje del muelle. |
| `Suspension_Lowered` | Suspension | Cylinder 14×14×18 | 8 | 2 | `SM_Suspension_Lowered` | 150–400 | Centro del muelle; Z = eje del muelle. |
| `Suspension_Raised` | Suspension | Cylinder 16×16×46 | 12 | 2 | `SM_Suspension_Raised` | 150–400 | Centro del muelle; Z = eje del muelle. |
| `Suspension_Broken` | Suspension | Cylinder 14×14×26 | 9 | 2 | `SM_Suspension_Broken` | 150–400 | Centro del muelle; Z = eje del muelle. |
| `Hood_Standard` | Hood | Cube 90×140×5 | 14 | 2 | `SM_Hood_Standard` | 150–500 | Centro de la cara inferior; Z hacia fuera. |
| `Hood_Scoop` | Hood | Cube 90×140×14 | 16 | 2 | `SM_Hood_Scoop` | 150–500 | Centro de la cara inferior; Z hacia fuera. |
| `Door_Standard` | Door | Cube 110×6×70 | 20 | 2 | `SM_Door_Standard` | 200–600 | Centro de la cara interior; Z arriba. |
| `Trunk_Standard` | Trunk | Cube 55×140×5 | 12 | 2 | `SM_Trunk_Standard` | 150–500 | Centro de la cara inferior. |
| `Bumper_Front` | Bumper | Cube 14×160×22 | 9 | 2 | `SM_Bumper_Front` | 150–500 | Centro de la cara de montaje. |
| `Bumper_Rear` | Bumper | Cube 14×160×22 | 9 | 2 | `SM_Bumper_Rear` | 150–500 | Centro de la cara de montaje. |
| `Spoiler_Race` | Spoiler | Cube 30×150×6 | 7 | 2 | `SM_Spoiler_Race` | 100–400 | Centro de la base; Z arriba (al revés = levanta el coche). |
| `Headlight_Standard` | Headlight | Sphere 22×22×22 | 2 | 1 | `SM_Headlight_Standard` | 100–250 | Centro de la base del faro. |
| `Taillight_Standard` | Taillight | Cube 6×26×14 | 1.5 | 1 | `SM_Taillight_Standard` | 100–200 | Centro de la base. |
| `Windshield_Standard` | Windshield | Cube 6×140×60 | 15 | 2 | `SM_Windshield_Standard` | 100–200 | Centro del cristal; Z = normal hacia arriba/afuera. |
| `Window_Standard` | Window | Cube 70×4×36 | 5 | 1 | `SM_Window_Standard` | 50–150 | Centro del cristal. |
| `Seat_Standard` | Seat | Cube 50×50×70 | 16 | 2 | `SM_Seat_Standard` | 300–800 | Centro del asiento a nivel de los anclajes. |
| `Seat_Bucket` | Seat | Cube 50×46×75 | 10 | 2 | `SM_Seat_Bucket` | 300–800 | Centro del asiento a nivel de los anclajes. |
| `SteeringWheel_Standard` | SteeringWheel | Cylinder 38×38×6 | 3 | 1 | `SM_SteeringWheel_Standard` | 150–400 | Centro del volante; Z = eje de la columna. |
| `Radio_Standard` | Radio | Cube 18×26×10 | 3 | 1 | `SM_Radio_Standard` | 100–250 | Centro de la base. |
| `Horn_Standard` | Horn | Cone 14×14×16 | 1 | 1 | `SM_Horn_Standard` | 100–300 | Base de la bocina. |
| `Horn_Ship` | Horn | Cone 30×30×50 | 6 | 1 | `SM_Horn_Ship` | 100–300 | Base de la bocina. |
| `Horn_Clown` | Horn | Sphere 18×18×18 | 1 | 1 | `SM_Horn_Clown` | 100–300 | Base de la bocina. |
| `Accessory_Horns` | Accessory | Cone 18×110×30 | 4 | 1 | `SM_Accessory_Horns` | 200–900 | Centro de la base de apoyo en el techo. |
| `Accessory_Float` | Accessory | Cylinder 120×120×30 | 6 | 1 | `SM_Accessory_Float` | 200–900 | Centro de la base de apoyo en el techo. |
| `Accessory_RoofRack` | Accessory | Cube 110×90×8 | 8 | 2 | `SM_Accessory_RoofRack` | 200–900 | Centro de la base de apoyo en el techo. |

Todas las piezas: `SOCKET_Grip` y `SOCKET_Bolt_01..N` (N = columna Tornillos). Colisión `UCX_` de 1–3 cajas/cápsulas.
Daño visual (ART_DIRECTION §8): variante `SM_<PartId>_Damaged` opcional (abollada / cristal roto).

## 3. Coches (`DT_CarModels`)

El chasis es una caja física (`ChassisExtent`) + cabina (`CabinExtent`, `CabinOffset`). El arte final es **solo el cuerpo sin
piezas desmontables** (`BodyMesh`), con un socket por hueco en la posición del JSON.

| Modelo | Chasis (semiextensión cm) | Huecos | Asset final | Tris del cuerpo |
|---|---|---|---|---|
| `Car_Tiny` (Utilitario diminuto) | 140×75×24 | 28 | `SM_Car_Tiny_Body` | 1.500–4.000 (coche completo ≤ 12.000) |
| `Car_Sedan` (Berlina familiar) | 175×82×26 | 28 | `SM_Car_Sedan_Body` | 1.500–4.000 (coche completo ≤ 12.000) |
| `Car_Van` (Furgoneta de reparto) | 205×90×34 | 28 | `SM_Car_Van_Body` | 1.500–4.000 (coche completo ≤ 12.000) |
| `Car_Pickup` (Pickup cansada) | 200×88×30 | 28 | `SM_Car_Pickup_Body` | 1.500–4.000 (coche completo ≤ 12.000) |
| `Car_Sport80s` (Deportivo ochentero) | 185×84×20 | 28 | `SM_Car_Sport80s_Body` | 1.500–4.000 (coche completo ≤ 12.000) |

Sockets del cuerpo: `SOCKET_<SlotName>` para cada hueco (`SOCKET_Wheel_FL`, `SOCKET_Engine`, `SOCKET_Door_L`, …),
`SOCKET_Seat` (conductor), `SOCKET_Exit_L`. Pivote en el centro de la caja del chasis. Colisión: 2 cajas (chasis + cabina).

## 4. Herramientas

| Herramienta | Placeholder | Asset final | Tris | Notas |
|---|---|---|---|---|
| Llave inglesa (`Wrench`) | Cubo rojo 55×10×5 | `SM_Tool_Wrench` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Llave de cruz (`TireIron`) | Cubo azul 60×30×5 | `SM_Tool_TireIron` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Destornillador (`Screwdriver`) | Cilindro amarillo 7×7×40 | `SM_Tool_Screwdriver` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Estetoscopio (`Stethoscope`) | Esfera cromo 18×18×8 | `SM_Tool_Stethoscope` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Escáner OBD (`Scanner`) | Cubo naranja 25×16×8 | `SM_Tool_Scanner` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Extintor (`Extinguisher`) | Cilindro rojo 22×22×55 | `SM_Tool_Extinguisher` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Manguera (`Hose`) | Cilindro lima 10×10×45 | `SM_Tool_Hose` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Esponja (`Sponge`) | Cubo amarillo 22×14×10 | `SM_Tool_Sponge` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Lijadora (`Sander`) | Cubo azul 30×18×16 | `SM_Tool_Sander` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |
| Pistola de pintura (`PaintGun`) | Cono blanco 20×20×35 | `SM_Tool_PaintGun` | 100–600 | Pivote y `SOCKET_Grip` en el mango; `SOCKET_Tip` en la punta (FX). Color de la herramienta = parámetro (cosmético ToolColor). |

## 5. Equipamiento y mobiliario del taller

| Asset | Placeholder | Asset final | Tris | Notas |
|---|---|---|---|---|
| Gato (`AJack`) | Cubo rojo 55×30×12 cm, 14 kg | `SM_…` | 200–2.000 | Pivote en la base; `SOCKET_Lift` en la cabeza. Colisión: 2 cajas. |
| Borriqueta | Cilindro gris | `SM_…` | 200–2.000 | Pivote en la base. |
| Elevador hidráulico (`AHydraulicLift`) | Dos columnas + brazos cinemáticos | `SM_…` | 200–2.000 | Brazos como mesh separado (se mueven en Z). Panel con `SOCKET_Panel`. |
| Grúa de motor (`AEngineCrane`) | Base amarilla 170×90×18 cm + pluma + gancho | `SM_…` | 200–2.000 | Gancho mesh separado con `SOCKET_Hook`. |
| Cabina de pintura (`APaintBooth`) | Tres paredes y techo de cubos (900×640×440 cm) | `SM_…` | 200–2.000 | Solo nivel 3+. Colisión solo paredes. |
| Estantería (`AShelfSlot`) | Balda gris con etiqueta de precio | `SM_…` | 200–2.000 | Etiqueta diegética como mesh separado. |
| Caja registradora (`ACashRegister`) | Caja amarilla con texto | `SM_…` | 200–2.000 | Pantalla/cajón como mesh separado. |
| Pizarra de encargos (`AJobBoard`) | Tablero con TextRender | `SM_…` | 200–2.000 | Superficie para texto diegético. |
| Zona de entrega (`ADeliveryBay`) | Rectángulo pintado en el suelo | `SM_…` | 200–2.000 | Decal o mesh plano. |
| Punto de venta (`ASellPoint`) | Cartel | `SM_…` | 200–2.000 | — |
| Contenedor de chatarra (`AScrapBin`) | Caja abierta | `SM_…` | 200–2.000 | — |
| Teléfono de la grúa (`ATowPhone`) | Caja pequeña en la pared | `SM_…` | 200–2.000 | — |
| Carteles del desguace (`AJunkyardOfferSign`) | Cartel con TextRender | `SM_…` | 200–2.000 | — |
| Terminal de ampliación (`AWorkshopUpgradeTerminal`) | Cartel | `SM_…` | 200–2.000 | — |
| Armario de ropa (`AWardrobe`) | Caja alta | `SM_…` | 200–2.000 | Puertas como mesh separado. |
| Panel de color de pared (`AWallColorPanel`) | Caja con muestras | `SM_…` | 200–2.000 | — |
| Cliente (`ACustomerNPC`) | Cápsula de color + bocadillo TextRender | `SM_…` | 200–2.000 | Mismo estilo que el mecánico, sin herramientas; murmullo sin idioma. |
| Paredes, suelo, puertas, luces del taller (`AWorkshopBlockout`) | Cajas con colores de la paleta | `SM_…` | 200–2.000 | Módulos de 100 cm por nivel 1–4 (cada nivel más grande y equipado). |
| Exterior: explanada, calle, circuito, rampa, cuesta, tierra, charco, conos, muro de neumáticos, farola derribable, desguace | Primitivas | `SM_…` | 200–2.000 | Kit modular; farola `AKnockableLamp` con base y poste separados. |

## 6. Materiales

| Asset | Estado | Notas |
|---|---|---|
| `M_Master` | ⬜ creado por script | `Tools/Scripts/create_master_material.py`. Parámetros: BaseColor, PaintColor, PaintAmount, Dirt, Rust, Wetness, FreshPaint, Damage, Highlight. Falta: máscaras de suciedad/óxido por textura (hoy es un tinte uniforme) y sombreado toon. |
| `M_PP_Outline` | ⬜ creado por script | Contorno por profundidad + halo en stencil 1. Prueba A/B con `htm.Outline`. |
| `MI_*` | ⬜ | Instancias de `M_Master` por familia (metal, goma, cristal, tela) con rugosidad propia. |

## 7. Efectos (`EHTMFX` → Project Settings → Effects)

| Clave | Placeholder (`AHTMPlaceholderFX`) | Final |
|---|---|---|
| `Smoke` | Bolas grises que suben | `NS_Smoke`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `Fire` | Bolas naranjas/amarillas que laten | `NS_Fire`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `Sparks` | Bolitas amarillas que saltan | `NS_Sparks`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `KOStars` | Estrellas (esferas amarillas) girando | `NS_KOStars`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `DustPoof` | Nube marrón | `NS_DustPoof`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `WaterSplash` | Gotas azules | `NS_WaterSplash`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `PaintSplash` | Gotas del color (User.Tint) | `NS_PaintSplash`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `Scrap` | Trozos grises | `NS_Scrap`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |
| `Confetti` | Bolitas de colores de la paleta | `NS_Confetti`: pocas partículas, formas grandes, estilo "bolas de dibujo". Parámetro `User.Tint`. |

## 8. Audio (pendiente: no hay nada implementado)

| Asset | Notas |
|---|---|
| Motores `SC_Engine_*` | Uno por motor (I3, V6, V8, eléctrico), con pitch por velocidad. Personalidad cartoon. |
| Bocinas `S_Horn_Standard/Ship/Clown` | Coincide con `SoundTag` de la pieza (hoy es un bocadillo de texto). |
| Golpes `S_Impact_Metal/Rubber/Body` | Por masa y velocidad del impacto. |
| Tornillos `S_Bolt_Tighten/Loosen` | Carraca exagerada. |
| Voces `S_Murmur_*` | Murmullo sin idioma para mecánicos y clientes; grito "¡EH!". |
| Radio `S_Radio_*` | Música relajada de taller (rasgo RadioStuckOn). |
| Avisos `S_Beep_Temp`, `S_Fire`, `S_Extinguisher`, `S_Hose`, `S_Sander`, `S_Spray` | Uno por herramienta/estado. |

## 9. Interfaz

| Asset | Placeholder | Final |
|---|---|---|
| `WBP_HUD` | `AMechanicHUD` (Canvas) | Iconos grandes redondeados; mantener la información actual (avisos por verbo, barra de mantener, reloj, dinero, encargos, marcadores de avería/pieza floja, hueco de encaje, bocadillos, KO/atrapado, panel de conducción, checklist y resumen). |
| `WBP_MainMenu` | `AHTMMenuHUD` (Canvas) | Mismas opciones; navegación con mando. |
| Fuente `F_HTM_Rounded` | Roboto del motor | Tipografía redondeada y gruesa **con glifos de €, ✓ ✗ y flechas** (hoy se evitan esos símbolos). |
| Iconos de botones `T_Btn_*` | Texto `[E / X]` | Iconos de teclado y mando. |
