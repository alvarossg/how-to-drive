# Pasos que requieren el editor

Claude Code no puede abrir Unreal, compilar ni crear `.uasset` binarios a mano. Todo lo que se puede automatizar
está en `Tools/Scripts/` (Python del editor). Aquí están los pasos que tienes que dar tú, en orden.

> **Estado honesto:** nada de este repositorio se ha compilado ni ejecutado todavía. El código se ha escrito contra
> la API de UE 5.4 con cuidado, pero lo primero es compilarlo y corregir lo que falle (ver `DEVLOG.md`).

---

## 1. Primera compilación

1. Instala **Unreal Engine 5.4** (Epic Games Launcher) y **Visual Studio 2022** con la carga de trabajo
   "Desarrollo de juegos con C++" (incluye el SDK de Windows y las herramientas de Unreal).
2. Clic derecho en `HowToMechanic.uproject` → **Generate Visual Studio project files**.
3. Abre `HowToMechanic.sln`, configuración **Development Editor / Win64**, y compila el proyecto `HowToMechanic`.
   - Si hay errores, cópialos tal cual a Claude Code: "compila con estos errores: …".
4. Abre `HowToMechanic.uproject`. Si pregunta por recompilar módulos, di que sí.

## 2. Generar assets (automático)

Al abrir el editor por primera vez, `Content/Python/init_unreal.py` detecta que faltan los `DT_*` y ejecuta
`Tools/Scripts/setup_project.py`, que crea:

| Asset | Script |
|---|---|
| `/Game/Data/DT_Parts, DT_CarModels, DT_Jobs, DT_Traits, DT_Cosmetics, DT_WorkshopLevels, DT_BuyerProfiles, DT_CarNames` | `import_data_tables.py` |
| `/Game/Data/DA_Tuning` | `create_tuning_asset.py` |
| `/Game/Materials/M_Master`, `/Game/Materials/M_PP_Outline` | `create_master_material.py` |
| `/Game/Maps/L_MainMenu`, `/Game/Maps/L_Workshop` | `create_maps.py` |

Si no se lanzó solo (o quieres repetirlo): **Output Log → Cmd → Python** y escribe
`py Tools/Scripts/setup_project.py`. Mira el Output Log: cada paso escribe `[HTM] …`.

Después **reinicia el editor** (así `DA_Tuning` y los `DT_*` sustituyen a los valores por defecto y al JSON).

Comprueba: **Project Settings → Game → How To Mechanic** muestra las rutas rellenas.

> Si más adelante cambias un JSON de `Content/Data`, vuelve a ejecutar `py Tools/Scripts/import_data_tables.py`.

## 3. Probar en PIE (cada fase)

1. Abre `L_Workshop` (es el mapa de inicio del editor).
2. Play → el desplegable ya está en **Net Mode: Play As Listen Server**, **Number of Players: 2**.
3. Consola (tecla `º`/`` ` ``) del anfitrión: `HTMHelp` lista los comandos.

| Fase | Cómo comprobar la aceptación |
|---|---|
| 1 | Pasaos la rueda suelta del suelo (E coge, Q lanza). Coged entre los dos el bloque motor (heavy: uno solo lo arrastra). Corred contra la caja del suelo para tropezar. Lanzaos cosas. Mirad las dos ventanas: la posición debe coincidir. |
| 2 | El coche de la plaza 1 tiene la rueda delantera izquierda pinchada. Llave de cruz (azul) en el banco. Clic derecho mantenido afloja tornillos, la rueda cae, E para cogerla. Coged una rueda de la estantería, soltadla junto al hueco (círculo verde en el HUD), clic izquierdo mantenido aprieta. Si dejáis tornillos sin apretar, el HUD muestra `2/4` amarillo parpadeando. |
| 3 | `HTMSpawnPart Engine_V8_Huge` y montadlo en el utilitario (con la grúa o entre dos) → caballitos. Dejad una rueda floja y salid a la recta: debe soltarse y rodar. Dos coches (`HTMSpawnCar`) chocando, cada uno conducido por un jugador. |
| 4 | Usad la caja registradora para abrir el día. Jugad hasta el resumen. `HTMEndDay` fuerza el cierre. |
| 5 | Tras jugar, mirad `Saved/Telemetry/situations_*.csv`: ≥ 5 situaciones distintas. El resumen del día lo muestra. |
| 6 | `htm.Outline 1` / `0` para la prueba A/B del contorno. |
| 7 | `HTMGiveMoney 5000` y `HTMUpgrade` fuera de jornada para ver los niveles 2-4. Pistola de pintura (nivel 2): clic derecho cambia de color. |

## 4. Online con Steam (fase 7)

El PIE no usa Steam. Para probarlo de verdad:

1. Steam abierto con una cuenta en cada PC (dos cuentas distintas, amigas entre sí).
2. Empaqueta (**Platforms → Windows → Package Project**) o lanza *Standalone Game* desde el editor
   (`UnrealEditor.exe HowToMechanic.uproject -game`).
3. Menú: **Abrir taller (online, invitar amigos)** en un PC. En el otro, **Buscar talleres de amigos** o acepta la
   invitación (overlay de Steam, `Mayús+Tab`, o `HTMInvite` en la consola del anfitrión).
4. Con AppId 480 (Spacewar) salen salas de otros juegos: el código las filtra por `HTM_GAME`. Para publicar,
   pon el AppId real en `Config/DefaultEngine.ini` → `[OnlineSubsystemSteam] SteamDevAppId`.
5. Sin Steam (o con `DefaultPlatformService=Null`) el menú funciona en LAN.

## 5. Cuando llegue el arte final

Las clases C++ ya funcionan con placeholders. Para sustituirlos:

### Personaje (`SK_Mechanic`)
1. Importa `SK_Mechanic` en `/Game/Art/Character/` con su esqueleto. Nombres de hueso que usa el código:
   `pelvis` y `spine_01` (`UActiveRagdollComponent::PelvisBone` / `UpperBodyBone`, editables).
2. Crea el **Physics Asset** (`PA_Mechanic`): cápsulas simples, cabeza grande (1/3 del cuerpo). Revisa límites
   angulares de cuello, hombros y cadera para que el tambaleo no se rompa.
3. Material de la cara: un hueco de material llamado **`Face`** con parámetros de textura `EyesTex`, `BrowsTex`,
   `MouthTex`. Crea un `UFaceExpressionSet` (`DA_FaceExpressions`) con las texturas de Happy, Effort, Scared, KO,
   Angry y Surprised.
4. Crea `ABP_Mechanic` con **Parent Class = `MechanicAnimInstance`**. El grafo solo lee sus variables
   (`GroundSpeed`, `MoveDirection`, `Stance`, `CarryMode`, `CarryArmsAlpha`, `bIsWorking`, `Wobble`, `LeanSide`,
   `bIsDriving`…). Sin lógica en Blueprint.
5. `py Tools/Scripts/create_blueprint_subclasses.py` crea `BP_MechanicCharacter`. En él: Mesh = `SK_Mechanic`,
   Anim Class = `ABP_Mechanic`, `Expression → Expression Set = DA_FaceExpressions`.
6. **Project Settings → How To Mechanic → Mechanic Character Class** = `BP_MechanicCharacter`.

### Piezas y coches
- Cada fila de `DT_Parts` tiene `Mesh`: asigna el `SM_` y deja `PlaceholderShape` como está (se ignora).
- Cada fila de `DT_CarModels` tiene `BodyMesh`. Los huecos (`Slots`) son transforms relativos al centro del chasis:
  si el modelo final cambia de proporciones, ajusta `Location` en el JSON y reimporta.
- Colisión simple (`UCX_`) en cada `SM_`; el pivote debe coincidir con el punto de montaje (ver `ASSET_LIST.md`).

### Efectos
- Crea los `NS_` y asígnalos en **Project Settings → How To Mechanic → Effects** con la clave = nombre del valor de
  `EHTMFX` (`Smoke`, `Fire`, `Sparks`, `KOStars`, `DustPoof`, `WaterSplash`, `PaintSplash`, `Scrap`, `Confetti`).
  El sistema recibe el color por el parámetro de usuario `User.Tint`.

### Input
- Los `IA_`/`IMC_` se crean en tiempo de ejecución. Si quieres remapear en el editor, crea `IMC_OnFoot` e
  `IMC_Driving` y asígnalos en **Project Settings → How To Mechanic → Input** (los nombres de acción que debe
  contener están en `Source/HowToMechanic/Character/HTMInputConfig.cpp`).
