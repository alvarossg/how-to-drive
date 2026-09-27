# Diseño técnico — How to Mechanic

Documento vivo. Describe **lo que hay en el código**, no un plan. Si cambias una clase importante, actualízalo.

- Motor: **Unreal Engine 5.4** (fijado en `HowToMechanic.uproject`).
- Un módulo C++: `HowToMechanic` (`Source/HowToMechanic/`). Includes relativos al módulo: `#include "Core/HTMTypes.h"`.
- Log: `LogHTM`. Prefijo de clases propias: `HTM` cuando el nombre sería genérico (`AHTMGameMode`).

---

## 1. Principios de arquitectura

| Regla (CLAUDE.md) | Cómo se cumple |
|---|---|
| Multijugador desde el día 1 | Todo actor de juego tiene `bReplicates`. PIE arranca por defecto como *listen server + 2 clientes* (`DefaultEditor.ini`). |
| Servidor autoritativo | Los clientes solo envían **intenciones** por RPC (`ServerGrab`, `ServerBeginHold`, `ServerSetDriveInput`…). Dinero, piezas, daño, tornillos, encargos y física de coches solo cambian en el servidor. |
| Un solo sistema de interacción | `UInteractionComponent` (en el personaje) + interfaz `IInteractable`. Coches, piezas, herramientas, gatos, elevadores, carteles, caja registradora, armarios, compañeros atrapados… todos implementan `IInteractable`. |
| Piezas = actores físicos | `ACarPart : AGrabbableActor`. Montada: adjunta al chasis sin física. Desmontada: objeto físico normal. |
| Presupuesto de física | `UHTMPhysicsBudgetSubsystem` congela objetos ligeros lejanos y duerme el exceso por encima de `MaxActiveBodies` (150 por defecto, en `DA_Tuning`). `htm.PhysicsBudget.Stats 1` lo muestra. |
| Datos fuera del código | `DT_*` (JSON en `Content/Data`) + `DA_Tuning` (`UHTMTuningData`). El código solo lleva valores por defecto de esa clase. |

---

## 2. Mapa de clases

```
Core/          Tipos, ajustes, datos, reglas de partida y utilidades transversales
  HTMTypes            Enums del juego (pesos, verbos, herramientas, categorías, averías, rasgos, situaciones §13…)
  HTMSettings         UDeveloperSettings: rutas de DT_*/DA_Tuning/materiales/mapas, override de personaje e IMC
  HTMTuningData       UPrimaryDataAsset (DA_Tuning): TODO el tuning del caos, con multiplicadores de "física caótica"
  HTMDataSubsystem    Carga los DT_*; en editor, si faltan, los construye desde Content/Data/*.json
  HTMGameMode         Jornada, compraventa, grúa, objetos perdidos, ampliación del taller, guardado
  HTMGameState        Dinero, reputación, nivel, día, encargos, última evaluación, resumen, opciones de sala
  HTMPlayerState      Cosméticos equipados y contadores de logros
  HTMPlayerController Avisos privados (ClientToast) y comandos HTM* de consola (solo anfitrión)
  HTMGameInstance     Opciones elegidas en el menú y último error de red
  HTMTelemetrySubsystem  Eventos y situaciones §13 a CSV (Saved/Telemetry)
  HTMPhysicsBudgetSubsystem  Presupuesto de cuerpos simulando
  HTMVisualLibrary / HTMPlaceholderFX / HTMPalette  Placeholders: formas básicas, material, paleta, FX "bolas de dibujo"

Character/
  MechanicCharacter           ACharacter: movimiento, posturas (de pie / agachado / tumbado), estados
                              (Normal, Stumbling, KO, Trapped, Driving), tropiezos, grito "¡EH!", cosméticos
  ActiveRagdollComponent      Physical Animation si hay SkeletalMesh+PhysicsAsset; muelle procedural si es placeholder
  MechanicExpressionComponent Caras por texturas intercambiables (UFaceExpressionSet)
  HTMInputConfig              IA_/IMC_ construidos en runtime (teclado + mando), sustituibles por assets

Interaction/
  IInteractable               CanInteract / GetInteractionText / GetHoldDuration / Interact / InteractHoldTick…
  InteractionComponent        Enfoque (sweep local), RPCs de intención, sujeción, arrastre pesado entre dos
  GrabbableActor              Objeto físico cogible (Light / Medium / Heavy por masa) con daño por impacto

Tools/        ATool (llaves, estetoscopio, escáner, extintor, manguera, esponja, lija, pistola de pintura),
              AWaterPatch (charcos), AJack, AHydraulicLift, AEngineCrane
Parts/        ACarPart, FPartDefinitionRow, FPartSurfaceState, ISurfaceTreatable, HTMSurface (lógica compartida)
Vehicle/      AModularCar (APawn), UModularVehicleMovement, UEngineTemperatureComponent, ACarFireActor,
              UCarFactory, ASpeedTrap, AKnockableLamp
Customers/    UJobDirectorComponent, FJobEvaluator, ACustomerNPC, AJobBoard, ADeliveryBay
Economy/      AShelfSlot, ACashRegister, AJunkyardOfferSign, ASellPoint, AScrapBin, ATowPhone
Progression/  AWorkshopBlockout (mundo placeholder por niveles), UHTMSaveGame, AWorkshopUpgradeTerminal,
              AWardrobe, AWallColorPanel, APaintBooth
UI/           AMechanicHUD (Canvas), AHTMMenuGameMode / AHTMMenuPlayerController / AHTMMenuHUD
Net/          UHTMSessionSubsystem (Online Subsystem: Steam o Null)
```

---

## 3. Interacción (fase 1)

**Flujo:** el cliente dueño hace un *sweep* esférico desde la cámara por el canal `Interaction`
(`ECC_GameTraceChannel1`) → `FocusedActor` (solo local, para el resalte y el HUD). Al pulsar, envía la
intención con el actor objetivo; el servidor **revalida** distancia, verbo y `CanInteract` antes de actuar.

| Verbo | Tecla / mando | Ejemplos |
|---|---|---|
| Grab | E / X | coger, soltar, arrancar una pieza sin tornillos |
| Use (mantener) | clic izq. / RT | apretar tornillo, bombear gato, arrancar motor, rescatar a un compañero |
| AltUse (mantener) | clic der. / LT | aflojar, freno de mano, bajar elevador, cambiar color de la pistola |
| Enter | F / Y | subir / bajar del coche |

- Acciones de "mantener": el **servidor** cuenta el tiempo (`TickServerHold`) y repite si `IsHoldRepeatable`
  (un tornillo tras otro). `HoldAlpha` se replica solo al dueño para la barra del HUD.
- Herramientas continuas (manguera, extintor, pistola) → `ServerToolTrigger(bActive, bAlt)`; el efecto se aplica
  en el servidor cada tick.

### Replicación de objetos cogidos

- **Ligero / medio:** el servidor apaga la física y **adjunta** el actor al punto de agarre del personaje
  (una mano / dos manos). Se replica con la *attachment replication* nativa; los clientes aplican el mismo estado
  físico en `OnRep_AttachmentReplication` (`AGrabbableActor::RefreshPhysicsState`). Así no hay dos
  simulaciones peleándose.
- **Pesado:** sigue simulando en el servidor. Cada portador tiene un `UPhysicsHandleComponent` en el servidor:
  uno solo lo **arrastra** (objetivo bajo, con fricción); con dos o más se **levanta**. El movimiento llega a los
  clientes por la replicación de movimiento de actores físicos (`bRepPhysics`), con el suavizado por defecto de
  Chaos.
- Al soltar una pieza cerca de su hueco, `ACarPart::TrySnapToNearbySlot` la encaja (imán suave).
- Lanzar = soltar + impulso en el servidor. El impacto contra personajes calcula un *impact score* →
  tambaleo / tropiezo / KO (`AMechanicCharacter::ReceiveImpact`). Con "empujones entre amigos" apagado, lo que
  lanza un compañero no tumba.

---

## 4. Coche modular (fase 2)

- `AModularCar` (APawn) = **chasis** (`UBoxComponent` raíz con física) + cabina soldada + **slots** definidos en
  `DT_CarModels` (`FCarSlotDefinition`: categoría, transform, pieza de serie, `ParentSlot`, `BlockedBySlot`,
  `bRequired`, `WheelIndex`).
- **Estado replicado por slot:** `TArray<FCarSlotState> Slots` (pieza montada, tornillos apretados, montada al
  revés). Es la **fuente única** de los tornillos: la pieza pregunta al coche.
- **Montar:** `MountPart` adjunta la pieza al chasis sin física y recalcula estadísticas. Orientación libre: si
  llega boca abajo respecto al hueco queda `bReversed` (escape al revés = más calor; alerón al revés = levanta el
  coche; motor al revés = marcha atrás).
- **Tornillos:** `TightenBolt` / `LoosenBolt` con la herramienta correcta (`RequiredTool`). Al aflojar el último,
  la pieza cae. Arrancar con 0 tornillos = `Grab`.
- **Dependencias:** quitar la pieza de un slot suelta las de los slots que lo tienen como `ParentSlot`
  (suspensión → rueda, parachoques → faros, puerta → ventanilla) y `BlockedBySlot` impide acceder
  (motor bajo el capó).
- **Piezas flojas:** en el servidor, cada 0,5 s, `P(desprenderse) = f(tornillos que faltan, velocidad, vibración,
  impactos) × DetachMult`. Se sueltan heredando la velocidad → la rueda sale rodando.
- **Gato (`AJack`)** aplica una fuerza de muelle en el punto de contacto y puede volcar si se empuja; el
  **elevador** mueve brazos cinemáticos; la **grúa de motor** engancha piezas pesadas.
- **Atrapado:** personaje tumbado bajo el chasis + el chasis baja sobre él → `SetTrapped`. Un compañero lo saca
  con *Use* mantenido (el propio personaje es `IInteractable`).

---

## 5. Conducción y ADR-001

### ADR-001 — Vehículo propio de *raycast* sobre Chaos en lugar de `AWheeledVehiclePawn` (Chaos Vehicles)

**Contexto.** El juego necesita que las ruedas, la suspensión y el motor sean **actores independientes que se
montan y desmontan en caliente**, que una rueda floja **se suelte en marcha**, que el coche funcione con 3 ruedas o
con ruedas de radios distintos y que las estadísticas salgan de las piezas montadas (GDD §7.6).

**Opciones de nuestra versión (UE 5.4):**

1. **Chaos Vehicles (`UChaosWheeledVehicleMovementComponent`).** Replicación de vehículo madura con predicción
   del cliente que conduce. Pero las ruedas son *configuración* del componente (`WheelSetups`, clases
   `UChaosVehicleWheel`) atadas a huesos de un `SkeletalMesh`: no son actores, no pueden desaparecer en marcha sin
   reconstruir la simulación, y cambiar su número o radio en runtime no está soportado de forma fiable.
2. **Física de actores con constraints** (ruedas como cuerpos unidos por `PhysicsConstraint`). Muy "físico", pero
   inestable a velocidad, caro en red (cada rueda es un cuerpo replicado) y difícil de ajustar.
3. **Raycast propio sobre el cuerpo Chaos del chasis** (elegida). Un único cuerpo rígido; por cada rueda
   *presente* se lanza un rayo, se calculan muelle-amortiguador, tracción, freno y agarre lateral (círculo de
   fricción), y se aplican **fuerzas en el punto de contacto**.

**Decisión:** opción 3 (`UModularVehicleMovement`). El plugin Chaos Vehicles sigue activado por si se quiere
probar la opción 1 para coches "de exhibición" sin piezas.

**Consecuencias:**
- ✔ Las piezas mandan: masa total, centro de masas (media ponderada por posición de cada pieza), par del motor,
  radio y agarre de cada rueda, altura y rigidez de cada suspensión, frenos, aerodinámica.
- ✔ Caballitos y vuelcos salen solos: fuerzas en el contacto + centro de masas alto/atrasado con un motor enorme.
- ✔ Una rueda que se suelta simplemente deja de lanzar rayo: la esquina cae y el coche rasca (chispas).
- ✘ No hay predicción en el cliente que conduce: el conductor remoto nota la latencia del servidor
  (ver §5.2). Aceptable para un party game a velocidades de taller; revisable.
- ✘ El giro visual de las ruedas y la compresión de la suspensión no se animan todavía (deuda en DEVLOG).

### 5.1 Qué se calcula de las piezas (`AModularCar::RecalculateStats`)

| Pieza | Efecto |
|---|---|
| Motor | par × condición × avería (golpeteo −25 %), velocidad máx., consumo, calor, refrigeración; al revés = sentido invertido |
| Escape | multiplicador de par (deportivo 1,2); al revés o con fuga = más calor |
| Rueda (por esquina) | radio (rueda pinchada/rota más pequeña), agarre; ruedas desiguales tiran hacia un lado |
| Suspensión (por esquina) | longitud en reposo, rigidez, amortiguación, **frenos**; sin suspensión = buje a tope |
| Alerón | carga aerodinámica (al revés: −1,5× → levanta) y resistencia |
| Bocina | existe o no; sonido (`SoundTag`: Ship, Clown…) |
| Todas | masa y posición → masa total y centro de masas |

### 5.2 Replicación de vehículos y física (pregunta de la fase 3)

Opciones en UE 5.4:

| Opción | Qué es | Veredicto |
|---|---|---|
| Replicación de movimiento por defecto (`bReplicateMovement` + `FRepMovement`) con *physics replication* por defecto | El servidor simula; los clientes reciben posición/velocidad y corrigen su cuerpo local suavemente | **Usada.** Simple, robusta, funciona igual para coches, piezas sueltas y objetos |
| Physics Prediction / *Resimulation* de Chaos (`PhysicsPrediction.bEnablePhysicsPrediction`, modos *PredictiveInterpolation* / *Resimulation*) | Predicción del cliente con resimulación | Experimental en 5.4. Desactivado (`DefaultEngine.ini`). Candidato si el conductor remoto nota demasiada latencia |
| Network Prediction plugin | Framework de predicción genérico | Experimental; mucho trabajo para este caso |
| Chaos Vehicles con su replicación | Predicción del conductor incluida | Descartado por ADR-001 |

**Decisión:** servidor autoritativo con replicación de movimiento estándar. El conductor envía su entrada
cuantizada (`ServerSetDriveInput`, `int8` ×3 + freno de mano) y el servidor simula. `NetUpdateFrequency` del
coche = 40 Hz. Varios coches chocando se resuelven en una única simulación (la del servidor), así que son
coherentes para todos.

### 5.3 Temperatura, humo y fuego

`UEngineTemperatureComponent`: `calor = HeatGeneration × carga × HeatMult`, `enfriamiento = Cooling × flujo de aire ×
(T − ambiente)`. Umbrales en `DA_Tuning`: aviso → humo → `ACarFireActor` (crece, daña piezas cercanas, asusta a
los personajes). El extintor lo apaga.

---

## 6. Bucle de jornada (fase 4)

- `EDayPhase`: **Closed** (preparar el taller) → **Open** (reloj en marcha, llegan clientes) → **Summary**
  (resumen) → Closed. Se cambia con la **caja registradora** (diegético) o con la consola.
- `UJobDirectorComponent` (en el GameMode): cola de clientes; número de encargos escalado por jugadores; al menos
  uno absurdo al día; el día 1 empieza con la rueda pinchada (`JOB_FLAT`).
- Coche entrante = modelo + estado aleatorio de piezas (condición, óxido, suciedad, tornillos flojos, piezas
  que faltan) + averías ocultas + 1-2 rasgos (`UCarFactory::SpawnCar`).
- **Evaluación automática** (`FJobEvaluator`): checks implícitos "Coche completo" y "Sin piezas flojas" + los del
  encargo + plazo. Pago completo / parcial / cliente enfadado. Resultado replicado en `LastEvaluation` y mostrado
  en la pizarra y el HUD.
- **Economía:** estantería (`AShelfSlot`, repone al empezar el día, piezas "Essential" primero), desguace
  (`FJunkyardOffer` con información incompleta), punto de venta (`ASellPoint`, precio según el comprador del día),
  chatarra, grúa de pago.
- **Guardado del anfitrión:** `UHTMSaveGame` (slot `HowToMechanic_Workshop`) al final de cada día y al ampliar:
  dinero, reputación, nivel, día, color de pared, cosméticos, coches del taller con piezas y superficies,
  decoración y cosméticos/logros por jugador.

---

## 7. Telemetría y tuning (fase 5)

- `UHTMTelemetrySubsystem` (solo servidor): `Saved/Telemetry/events_<fecha>.csv` y `situations_<fecha>.csv`.
- Situaciones de GDD §13 (`EHTMSituation`) detectadas por sistemas (no por guion) y registradas con 3 s de
  enfriamiento por tipo. El resumen del día enseña cuántas distintas hubo (aceptación: ≥ 5) y los "desastres del
  día".
- `DA_Tuning` agrupa por categorías: Character, Interaction, Chaos, Parts, Engine, Vehicle, Paint, Economy, Day,
  Physics Budget y **Chaotic Mode** (multiplicadores activados por la opción de sala).

## 8. Arte y placeholders (fase 6)

- `M_Master` (creado por script) con parámetros de estado: `BaseColor, PaintColor, PaintAmount, Dirt, Rust,
  Wetness, FreshPaint, Damage, Highlight`. Mientras no exista, se usa `BasicShapeMaterial` con el color compuesto
  en CPU (`HTMVisualLibrary::ApplySurface`), así que el juego se ve igual de legible.
- Contorno A/B: `htm.Outline 0/1` activa `M_PP_Outline` (tinta por profundidad + halo en stencil 1).
- FX: `UHTMSettings::Effects` (`EHTMFX` → `UNiagaraSystem`). Sin sistema asignado → `AHTMPlaceholderFX` (bolas
  de primitivas animadas, local, no replicado; lo disparan multicast o `OnRep`).

## 9. Online (fase 7)

- `UHTMSessionSubsystem` sobre el Online Subsystem por defecto (`DefaultPlatformService=Steam`, AppId 480 de
  pruebas). Sin Steam → Null (LAN). Crear sala (lobby Steam) → `OpenLevel(L_Workshop?listen&…)`; buscar →
  `JoinSession` → `ClientTravel`. Invitaciones del overlay de Steam → `OnSessionUserInviteAccepted` → unirse.
- Net driver: SteamSockets con respaldo IpNetDriver.
- Los errores de red vuelven al menú con el motivo (`UHTMGameInstance::LastErrorMessage`).

## 10. Rendimiento y límites conocidos

- ≤ 150 cuerpos simulando (ajustable). Objetos ligeros lejanos (> `FreezeDistance`) se congelan; los que superan
  el presupuesto se duermen empezando por los más alejados de los jugadores.
- Coches: 4 rayos por coche y tick en el servidor. Piezas montadas no simulan.
- Clientes: no simulan coches ni objetos (solo corrigen hacia el servidor).
