# DEVLOG

Registro de lo hecho, decisiones y deuda técnica. Lo más reciente arriba.

---

## Estado por fase

| Fase | Código escrito | Compilado | Aceptación comprobada |
|---|---|---|---|
| 0 Arranque | ✅ | ❌ | ❌ |
| 1 Personaje + coger | ✅ | ❌ | ❌ |
| 2 Coche modular + taller | ✅ | ❌ | ❌ |
| 3 Conducción + zona de pruebas | ✅ | ❌ | ❌ |
| 4 Vertical slice | ✅ | ❌ | ❌ |
| 5 Telemetría + tuning | ✅ (el análisis de playtests requiere playtests) | ❌ | ❌ |
| 6 Dirección de arte (placeholders + materiales por script) | ✅ | ❌ | ❌ |
| 7 Progresión + online | ✅ | ❌ | ❌ |

**Importante:** el entorno donde se escribió no tiene Unreal Engine. **Nada se ha compilado ni ejecutado.** Todas las
casillas de aceptación siguen abiertas. El primer paso es `docs/EDITOR_STEPS.md` §1–3: compilar, corregir los errores
que salgan y comprobar cada fase en PIE con 2 jugadores.

---

## Sesión 1 — Todas las fases de una vez

### Contexto y desviaciones del proceso

- El usuario pidió generar **todas las fases a la vez, sin parar entre fases y sin preguntar**. Por eso no se
  esperó la confirmación del diseño técnico que piden CLAUDE.md y las fases 0, 2, 3 y 7; el diseño quedó por escrito
  en `docs/TECH_DESIGN.md` para revisarlo a posteriori.
- "Compila después de cada cambio" no se pudo cumplir (sin motor). El código se escribió contra la API de UE 5.4 y se
  revisó a mano: sin variables que oculten miembros (UE lo trata como error), includes relativos al módulo, RPCs con
  `_Implementation`, cabeceras sin métodos sin definir (comprobado por script).

### Qué se hizo

**Fase 0.** `.uproject` (UE 5.4; plugins Enhanced Input, Chaos Vehicles, Niagara, Python, Editor Scripting, Online
Subsystem Steam/Null, SteamSockets). Módulo `HowToMechanic` con las carpetas de CLAUDE.md. `.gitignore`,
`.gitattributes` (LFS para binarios de Unreal y audio; imágenes solo en `Art/**`). Configuración: canal de colisión
`Interaction`, subpasos de física, `r.CustomDepth=3`, PIE como listen server con 2 clientes, Steam AppId 480.

**Fase 1.** `AMechanicCharacter` (placeholder de 120 cm con cabeza 1/3, posturas de pie/agachado/tumbado, estados
Normal/Stumbling/KO/Trapped/Driving, tropiezos con objetos del suelo, impactos → tambaleo/KO de 2-3 s, grito "¡EH!"
con bocadillo, emotes). `UActiveRagdollComponent` (Physical Animation con malla final; muelle procedural con el
placeholder). `UInteractionComponent` + `IInteractable`. `AGrabbableActor` con clases de peso por masa (ligero < 8 kg,
medio < 45 kg, pesado ≥ 45 kg). Pesados: arrastre en solitario, levantar entre dos con `UPhysicsHandleComponent`.
Input por Enhanced Input creado en runtime (teclado + mando). Resalte con Custom Depth + parámetro `Highlight`.

**Fase 2.** `AModularCar` con huecos desde `DT_CarModels`, estado de huecos replicado (`FCarSlotState`: pieza,
tornillos, al revés). `ACarPart` como actor físico independiente con condición, superficie, avería oculta. Montaje por
imán con orientación libre; tornillos con la herramienta correcta; última tuerca = la pieza cae. Dependencias entre
huecos (padre / bloqueado por). `AJack` (fuerza de muelle en el contacto, puede volcar), borriquetas, jugador atrapado
bajo el coche y rescate. Taller nivel 1 (blockout procedural) con plaza, estantería, banco de herramientas.

**Fase 3.** `UModularVehicleMovement` (ADR-001: raycast propio sobre Chaos). Estadísticas desde piezas; fuerzas en el
punto de contacto → caballitos y vuelcos. Piezas flojas que se desprenden por velocidad, vibración e impactos; daño por
choques. `UEngineTemperatureComponent` → humo → `ACarFireActor` → extintor. Motor encendido y ruedas empujan a quien
esté cerca. Zona de pruebas: calle, recta con trampa de velocidad, cuesta, rampa, tierra, charco, conos, muro de
neumáticos, farola derribable. Volcar y enderezar empujando entre varios. Grúa de pago.

**Fase 4.** Jornada (cerrado → abierto → resumen) con la caja registradora. `UJobDirectorComponent`: 15 encargos
(6 absurdos), escalados por jugadores, plazo, evaluación automática con checklist, pago completo/parcial/enfadado y
reputación. Pizarra diegética. Averías ocultas y diagnóstico con estetoscopio (ruidosas) y escáner (todas). Compra en
el desguace con información incompleta, estantería con reposición, venta según el comprador del día, chatarra.
Guardado del anfitrión. Resumen de fin de día con "desastres del día". Coches entrantes = modelo + estado + averías +
1-2 rasgos.

**Fase 5.** `UHTMTelemetrySubsystem` → CSV (eventos con duración de encargo y situaciones §13). `DA_Tuning` con todo
el tuning del caos y la opción de sala "física caótica". Contador de situaciones distintas en el resumen.

**Fase 6.** Paleta oficial en código (`HTMPalette.h`) y en `ART_DIRECTION.md`. `M_Master` y `M_PP_Outline` por script;
prueba A/B `htm.Outline`. Superficies como estado de juego (suciedad, óxido, mojado, pintura, pintura fresca, daño).
Caras por texturas (`UFaceExpressionSet`). FX placeholder "bolas de dibujo" sustituibles por Niagara. UI diegética:
pizarra, caja registradora, etiquetas de precio, carteles del desguace. `ASSET_LIST.md` con especificaciones.

**Fase 7.** Niveles de taller 1-4 (`DT_WorkshopLevels`: tamaño, plazas, estantería, borriquetas, elevador, grúa de
motor, cabina de pintura, escáner). Pistola de pintura con secado, estropeo por agua/suciedad, lavado con
manguera/esponja, lijado. Cosméticos (gorro, ropa, guantes, color de herramientas; 5 por logros absurdos) y color de
pared del taller. `UHTMSessionSubsystem` (crear, buscar, unirse, invitar con Steam; LAN de respaldo). Menú principal.
Guardado persistente del taller con coches, piezas y decoración.

**Extra:** HUD Canvas completo como placeholder de los `WBP_`, controlador con comandos de consola del anfitrión,
`UMechanicAnimInstance` para el futuro ABP, scripts de editor y setup automático al primer arranque.

### Decisiones

- **ADR-001:** vehículo propio de raycast en vez de Chaos Vehicles (ver `TECH_DESIGN.md` §5).
- **Replicación de física:** servidor autoritativo con la replicación de movimiento estándar; predicción de física de
  Chaos desactivada (experimental en 5.4).
- **Objetos cogidos:** ligeros y medios se adjuntan sin física (attachment replication); pesados simulan en el
  servidor con Physics Handles.
- **Tornillos en el coche, no en la pieza:** una sola fuente de verdad replicada.
- **Datos en JSON importable** + respaldo en editor que construye tablas transitorias desde el JSON, para poder jugar
  antes de importar nada.
- **UI con Canvas** hasta tener arte: funcional en red y sin assets binarios.
- **Glifos:** los textos evitan €, ✔, flechas, notas musicales y comillas tipográficas porque las fuentes por defecto
  pueden no tenerlos (se usa `EUR`, `[OK]`, `« »`…).
- **Borriquetas desde el nivel 1** (lo pide la fase 2).

### Deuda técnica y riesgos

1. **Sin compilar.** Esperables errores de compilación menores (firmas de API, includes). Prioridad absoluta.
2. **Sin predicción del conductor remoto:** notará la latencia del servidor al conducir. Si molesta, probar
   `PhysicsPrediction` (resimulación de Chaos) o predecir solo la entrada.
3. Las ruedas no giran visualmente y la suspensión no se comprime en pantalla (la física sí).
4. `M_Master` tiñe de forma uniforme: faltan máscaras de suciedad/óxido por textura y el sombreado toon.
5. **Sin audio.** Bocinas, radio y avisos son bocadillos de texto.
6. HUD y menú en Canvas, no UMG; sin fuente propia ni iconos de mando.
7. El blockout del taller se construye en `BeginPlay`: en el editor el mapa se ve vacío salvo el actor.
8. Telemetría solo local (CSV). No hay herramienta de análisis: se abre en una hoja de cálculo.
9. Los tamaños de los huecos de los coches están pensados para placeholders; con el arte final habrá que reajustar el
   JSON.
10. Steam con AppId 480 (pruebas). Falta un AppId propio para publicar.
11. No hay repeticiones ni cámara de "desastre del día" (el resumen es texto).

### Cómo probar

Ver `docs/EDITOR_STEPS.md` §3: tabla de comprobación de la aceptación de cada fase en PIE (listen server + 2 clientes)
y comandos de consola (`HTMHelp`).
