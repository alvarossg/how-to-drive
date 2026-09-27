# Prompts por fases

Hoja de ruta: `reference/32_hoja_de_ruta.png`.

```
0 Arranque → 1 Personaje + coger → 2 Coche modular → 3 Conducción → 4 Vertical slice → 5 Playtest → 6 Arte → 7 Progresión + online
                                                                     ▲ ¿Es divertido? Decide aquí
```

La fase 4 es el punto de decisión: si el bucle no es divertido con cubos grises, el arte no lo arreglará.

**Cómo usarlo:**
1. Crea un proyecto C++ vacío en Unreal Engine 5 llamado `HowToMechanic` y ponlo bajo git (con Git LFS para los
   `.uasset`). *(En este repositorio ya está creado el esqueleto: ver `README.md`.)*
2. `CLAUDE.md` en la raíz; `GDD.md` y `ART_DIRECTION.md` en `docs/`.
3. Abre Claude Code en esa carpeta y pega los prompts de uno en uno. No pases a la siguiente fase hasta cumplir
   los criterios de aceptación.

Estado de cada fase: ver `docs/DEVLOG.md`.

---

## FASE 0 — Arranque

```
Lee CLAUDE.md, docs/GDD.md y docs/ART_DIRECTION.md.
Después:
1. Resume en 10 líneas qué has entendido del juego y señala cualquier contradicción o duda.
2. Revisa la estructura del proyecto y configura: módulos C++, carpetas de Source según
   CLAUDE.md, .gitignore y .gitattributes para Unreal con LFS, y los plugins necesarios (Chaos
   Vehicles, Enhanced Input, Python Editor Script, Niagara).
3. Crea docs/DEVLOG.md y docs/ASSET_LIST.md vacíos con su plantilla.
4. Propón el diseño técnico de las fases 1 y 2 (clases, responsabilidades, qué se replica) y
   espera mi confirmación antes de programar.
```

**Aceptación:** el proyecto compila, abre en el editor y el diseño técnico está aprobado.

## FASE 1 — Personaje físico y sistema de coger (ya en multijugador)

```
Implementa el personaje jugable y el sistema de interacción, replicado desde el principio:
- Personaje en tercera persona con Enhanced Input (mando y teclado): andar, correr, saltar,
  agacharse/tumbarse.
- Active ragdoll con Physical Animation Component: se tambalea al recibir golpes, tropieza con
  objetos del suelo, queda KO 2-3 s con impactos fuertes y se levanta solo.
- UInteractionComponent + IInteractable: coger/soltar/lanzar objetos ligeros (una mano), medios
  (dos manos, más lento) y pesados (arrastre en solitario, transporte entre dos jugadores).
- Objetos de prueba placeholder según la paleta de ART_DIRECTION: caja, rueda, llave inglesa,
  bloque "motor".
- Resalte de objeto interactuable y botón "¡EH!" con bocadillo.
- Mapa gris de pruebas con suelo, rampa y obstáculos.
Todo con servidor autoritativo. Explícame cómo has resuelto la replicación de objetos cogidos y
de físicas.
```

**Aceptación:** dos jugadores en PIE pueden pasarse una rueda, cargar un motor entre los dos, tropezar con una
caja y tirarse cosas, sin desincronizaciones visibles.

## FASE 2 — Coche modular y el taller mínimo

```
Implementa el sistema de piezas y slots descrito en GDD sección 7:
- Clase de coche con slots definidos por DataAsset. Un modelo placeholder (utilitario diminuto)
  hecho de primitivas, con cada pieza como actor separado.
- Piezas con condición, estado, masa y estadísticas (DataTable).
- Desmontar: aflojar tornillos con la herramienta correcta manteniendo botón; la pieza cae como
  objeto físico.
- Montar: snap magnético al hueco, orientación libre (se puede montar al revés), y tornillos a
  apretar. Piezas no apretadas del todo quedan "flojas".
- Gato que levanta una esquina (el coche puede caerse si se empuja) y borriquetas.
- Jugador tumbado bajo el coche; si el coche baja sobre él, queda atrapado hasta que lo
  liberen.
- Taller mínimo (nivel 1) en gris: una plaza, estantería con piezas de repuesto y herramientas.
Propón primero el diseño de clases y cómo replicas el estado de cada slot.
```

**Aceptación:** dos jugadores cambian una rueda pinchada en cooperación, y una rueda sin apretar se nota como floja.

## FASE 3 — Conducción y zona de pruebas

```
Integra Chaos Vehicles con el sistema de piezas:
- Las estadísticas de conducción se calculan de las piezas montadas (masa total, centro de
  masas, par del motor, tamaño de rueda, altura de suspensión).
- Las piezas flojas se desprenden según vibración, velocidad e impactos. Las piezas pueden
  dañarse por choques.
- Temperatura del motor: sobrecalentamiento → humo → pequeño fuego (placeholder Niagara) que se
  apaga con extintor.
- Cualquier jugador puede arrancar el coche en cualquier momento; motor y ruedas empujan a
  quien esté cerca.
- Zona de pruebas compacta pegada al taller: calle, cuesta, rampa, tierra, charco, conos, muro
  de neumáticos, farola derribable.
- Varios coches conduciendo a la vez en red. Volcar y empujar para enderezar. Grúa de pago.
Antes de programar, dime qué opciones de replicación de vehículos y física tiene nuestra
versión de Unreal y cuál recomiendas.
```

**Aceptación:** meter un motor enorme en el coche pequeño provoca caballitos; una rueda floja sale rodando durante
la prueba; dos coches pueden chocar en red de forma creíble.

## FASE 4 — Bucle completo (vertical slice)

```
Cierra el primer bucle jugable de 20 minutos:
- Jornada con inicio y cierre de día.
- Encargos de clientes medibles (GDD sección 10): pizarra diegética, checklist de evaluación
  automática, plazo opcional, pago y reputación. Mínimo 6 encargos, 2 de ellos absurdos.
- Problemas ocultos y diagnóstico con estetoscopio.
- Compra de coches en el desguace y de piezas en la estantería; venta de coches.
- Dinero guardado por el anfitrión.
- Resumen de fin de día.
Genera los coches entrantes combinando modelo + estado de piezas + 1-2 rasgos.
```

**Aceptación:** un grupo de 2–4 personas juega un día completo sin explicaciones externas y quiere repetir.

## FASE 5 — Playtest y ajuste del caos

```
Ayúdame a preparar y analizar playtests:
- Añade telemetría local simple (eventos: pieza caída, KO, coche volcado, encargo fallado,
  tiempo por encargo) exportada a CSV.
- Crea un panel de tuning (DataAsset) con los parámetros clave del caos: fuerza para tropezar,
  umbral de KO, probabilidad de desprendimiento de piezas flojas, temperatura del motor.
- Tras cada playtest que te describa, propón ajustes concretos según la tabla de GDD sección 13
  y las reglas anti-frustración de la sección 14.
```

**Aceptación:** en cada playtest aparecen al menos 5 de las situaciones de la tabla de la sección 13 sin forzarlas.

## FASE 6 — Aplicar la dirección artística a la slice

```
Aplica ART_DIRECTION.md a todo lo que existe:
- Material maestro con parámetros de color, suciedad, óxido, pintura y daño; instancias para
  todos los objetos.
- Iluminación y postproceso según la sección 10; prepara una prueba A/B con y sin contorno.
- Sistema de expresiones faciales por texturas intercambiables.
- Efectos Niagara placeholder con el estilo descrito.
- UI diegética: pizarra, caja registradora, etiquetas de precio.
- Actualiza ASSET_LIST.md con cada asset que falte de arte final, sus especificaciones
  (triángulos, sockets, pivote, colisión) para pasárselo a un artista.
Revisa cada asset con la checklist de la sección 15.
```

**Aceptación:** una captura cualquiera del juego se ve coherente, colorida y legible.

## FASE 7 — Progresión, personalización y online real

```
Implementa:
- Niveles del taller 2 y 3 (GDD sección 11) con mejoras que cambian el flujo: elevador
  hidráulico, grúa de motor, cabina de pintura, escáner.
- Pintura con pistola sobre la carrocería, tiempo de secado y estropeo por agua/suciedad;
  lavado y lijado.
- Personalización cosmética del personaje y del taller.
- Sesiones online con Steam (Online Subsystem): crear partida, invitar amigos, unirse.
- Guardado persistente del taller.
Propón el orden y el diseño antes de empezar.
```
