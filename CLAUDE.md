# CLAUDE.md — How to Mechanic

Claude Code lee este archivo automáticamente en cada sesión. Son las reglas permanentes del proyecto.

## Qué estamos haciendo

Party game cooperativo basado en física, de 1 a 4 jugadores online, sobre un taller de coches cutre que
crece hasta convertirse en un imperio. La diversión sale de la física, de la interacción entre jugadores y
de sus decisiones, no de eventos guionizados.

Documentos de referencia (mandan sobre cualquier suposición):

- `docs/GDD.md` — qué es el juego.
- `docs/ART_DIRECTION.md` — cómo se ve.
- `docs/PROMPTS_POR_FASES.md` — orden de trabajo y criterios de aceptación.
- `docs/TECH_DESIGN.md` — diseño técnico vigente (clases, replicación, decisiones).
- `docs/DEVLOG.md` — qué se hizo, decisiones y deuda técnica.
- `docs/ASSET_LIST.md` — placeholders y assets pendientes de arte final.
- `docs/EDITOR_STEPS.md` — pasos que requieren el editor y no puede hacer Claude Code.
- `docs/reference/` — imágenes de referencia numeradas.

## Stack técnico

- Unreal Engine 5 (la versión queda fijada en el `.uproject`; no la actualices sin que te lo pida).
- C++ para toda la lógica de juego. Los Blueprints se usan solo como subclases de clases C++ para asignar
  meshes, sonidos, efectos y valores de tuning. Motivo: los `.uasset` son binarios y no puedes editarlos de
  forma fiable.
- Chaos Physics, Chaos Vehicles (ver ADR-001 en `docs/TECH_DESIGN.md`), Enhanced Input, Animation
  Blueprints (con la lógica en un `UAnimInstance` de C++), Physical Animation Component, Niagara.
- Replicación nativa de Unreal, modelo listen server. Online Subsystem (Steam / EOS) en una fase posterior.
- Datos de juego en DataAssets y DataTables (importables desde CSV/JSON), nunca números mágicos en el código.
- Creación o modificación de assets en lote mediante scripts Python del editor guardados en `Tools/Scripts/`.

## Qué puedes hacer y qué no

- **Sí puedes:** escribir C++, `.ini`, scripts Python de editor, CSV/JSON de datos, materiales creados por
  script, documentación.
- **No puedes** producir arte final (modelos, rigs, animaciones, texturas). Usa placeholders coherentes con la
  dirección artística (formas simples, colores planos de la paleta oficial) y deja preparados los nombres,
  sockets y rutas para sustituirlos. Registra cada placeholder en `docs/ASSET_LIST.md`.
- Si algo requiere el editor (crear un Blueprint, ajustar un Physics Asset, pintar pesos), da los pasos
  exactos en `docs/EDITOR_STEPS.md`. Nunca digas que algo está hecho si no lo has podido hacer tú.

## Arquitectura obligatoria

- **Multijugador desde el primer día.** Nada se programa "primero en local y luego lo replicamos". Todo
  sistema se prueba con 2 jugadores en PIE (Play As Listen Server, 2 players).
- **Servidor autoritativo.** Dinero, piezas, daño, estado de coches y encargos cambian solo en el servidor.
  Los clientes envían intenciones por RPC.
- **Un único sistema de interacción** (`UInteractionComponent` + interfaz `IInteractable`) para todo: coger,
  soltar, lanzar, usar herramienta, atornillar, subir al coche.
- **Las piezas del coche son actores físicos independientes** que se acoplan a slots del coche. Una pieza
  desmontada es un objeto físico más del mundo.
- **Presupuesto de física:** limita los cuerpos simulando a la vez (empieza con ≤150 y ajústalo midiendo).
  Los objetos en reposo duermen; los pequeños y lejanos dejan de simular.
- **Estructura de carpetas:** `Source/HowToMechanic/{Core, Character, Interaction, Tools, Parts, Vehicle,
  Economy, Customers, Progression, UI, Net}`.
- **Convenciones de nombres** estándar de Unreal (prefijos A, U, F, E, I; assets con prefijo SM_, SK_, M_,
  MI_, NS_, DA_, DT_, BP_, WBP_).

## Cómo trabajar conmigo

1. Trabaja por fases (`docs/PROMPTS_POR_FASES.md`). No empieces una fase hasta que la anterior cumpla sus
   criterios de aceptación.
2. Antes de un sistema grande, propón un diseño breve (clases, responsabilidades, qué se replica y cómo) y
   espera mi confirmación.
3. Compila después de cada cambio. Nunca dejes el proyecto sin compilar.
4. Al terminar una tarea, dime cómo la has probado o exactamente cómo debo probarla yo.
5. Commits pequeños con mensajes claros.
6. Mantén `docs/DEVLOG.md` al día: qué se hizo, decisiones tomadas y deuda técnica.

## Principios de diseño innegociables

1. **El caos sale de sistemas, no de guiones.** Si un momento gracioso solo puede ocurrir con una animación o
   evento preparado, replantea el sistema.
2. **El caos tiene que ser legible y recuperable.** El jugador debe entender qué ha pasado y poder arreglarlo.
   Frustración ≠ diversión.
3. **Interacción simple, consecuencias físicas.** Si hacer algo con física pura es frustrante (por ejemplo,
   atornillar), simplifica la acción y deja que la física gobierne las consecuencias (la rueda mal apretada
   sale volando en la prueba).
4. **Legible a distancia y en una captura.** Todo lo importante debe entenderse sin leer texto.

## Notas prácticas del repositorio

- Módulo único de juego: `HowToMechanic`. Categoría de log: `LogHTM`.
- Datos fuente en texto: `Content/Data/*.json` y `*.csv`. Se convierten a `DT_*` con
  `Tools/Scripts/import_data_tables.py`. Si cambias un struct `F*Row`, actualiza el JSON y reimporta.
- Rutas de datos configuradas en `Config/DefaultGame.ini` → `[/Script/HowToMechanic.HTMSettings]`.
- Toda cifra de tuning del caos vive en `UHTMTuningData` (`DA_Tuning`). No metas números en el código salvo
  como valor por defecto de esa clase.
