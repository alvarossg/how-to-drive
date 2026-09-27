# How to Mechanic — Documento de diseño (GDD)

> Party game cooperativo de física para 1–4 jugadores · Unreal Engine 5
>
> *"El juego donde tus amigos intentan reparar un coche y todo acaba saliendo mal."*

Referencias visuales: `docs/reference/` (ver índice al final).

## 1. Pitch

Un taller de coches destartalado, tú y hasta 3 amigos. Compráis chatarra, la arregláis (o lo intentáis), la
probáis y la vendéis. Empezáis arreglando basura en un garaje de mala muerte y acabáis con un imperio del motor.

**Filosofía:** "Puedes intentar hacer el trabajo perfectamente, pero probablemente algo saldrá mal."

La frase que queremos oír al final de cada partida: **"¿QUIÉN HA DEJADO LA RUEDA EN MEDIO?"**

Ilustración: `reference/20_ilustracion_martes_en_el_taller.png` — un martes cualquiera en el taller: motor
volando, rueda escapando, coche mal apoyado en el gato y alguien debajo.

## 2. Pilares (toda decisión se mide contra ellos)

1. **Física que genera historias.** Los momentos memorables emergen de sistemas, no de guiones.
2. **Fácil de entender, difícil de hacer bien.** Cualquiera entiende cambiar una rueda en 10 segundos; hacerlo
   rápido, bien y sin romper nada en equipo es otra historia.
3. **Cooperación con fricción.** Los jugadores se necesitan y a la vez se estorban.
4. **Progreso visible.** El taller, los coches y los personajes cambian a ojos vista.

## 3. Datos básicos

| | |
|---|---|
| Plataforma inicial | PC (Steam). Consolas más adelante. |
| Jugadores | 1–4 online (listen server). Debe ser jugable en solitario: los encargos escalan con el número de jugadores. |
| Cámara | Tercera persona, cámara libre detrás del personaje. |
| Controles | Mando y teclado/ratón desde el principio. |
| Duración de sesión | 30–90 min. Un coche típico: 5–15 min de trabajo. |
| Guardado | Progreso del taller persistente, guardado por el anfitrión. |

## 4. Bucles de juego

- **Momento a momento (segundos):** coger, transportar, colocar, apretar, tropezar, lanzar, gritar.
- **Trabajo (5–15 min):** llega un coche → diagnosticar → desmontar → reparar/sustituir/modificar → probar →
  entregar o vender.
- **Jornada (15–25 min):** un día en el taller con varios encargos, compras de piezas y coches. Al cerrar,
  resumen de dinero y reputación con repetición de los mejores desastres.
- **Meta (horas):** ampliar el taller, desbloquear herramientas, zonas, piezas y clientes más exigentes.

```
 1 Llega coche → 2 Diagnosticar → 3 Desmontar → 4 Reparar/modificar → 5 Probar fuera → 6 Entregar o vender
                                                                          └── ¡Algo falla! ──┘
```

Los cuatro bucles encajados. El trabajo siempre termina en la prueba, y la prueba es donde se descubre lo que
salió mal.

## 5. El personaje

- Personaje con **active ragdoll**: animación normal mezclada con física. Pierde el equilibrio si le golpea
  algo pesado, tropieza con objetos del suelo y cae de forma cómica.
- **No hay muerte.** Un golpe fuerte deja al jugador KO 2–3 segundos (estrellitas) y se levanta solo.
- **Acciones:** andar, correr, saltar, agacharse/tumbarse (para meterse bajo el coche), coger, soltar, lanzar,
  empujar, subir/bajar del coche, usar herramienta, gesto/emote, gritar (botón de "¡EH!" con bocadillo, útil
  sin chat de voz).
- Variantes: base, gorra, casco, gafas de taller, gorro, ropa alternativa. Mismo esqueleto: la ropa y los
  gorros son piezas intercambiables.
- Expresiones por textura intercambiable (ojos + cejas + boca): feliz, esfuerzo, susto, KO, enfado, sorpresa.
  Sin rig facial complejo.

Referencia: `reference/21_personaje_variantes_expresiones.png`, `reference/02_personajes.jpg`.

## 6. Interacción física (el corazón del juego)

- **Objetos ligeros** (tornillos, herramientas, luces): una mano, se pueden lanzar.
- **Objetos medios** (ruedas, puertas, parachoques): dos manos, el personaje va más lento y ve peor.
- **Objetos pesados** (motor, caja de cambios): requieren dos jugadores o una herramienta (carrito, grúa de
  motor). Un jugador solo puede arrastrarlos por el suelo, lentamente y con riesgo.
- Todo lo que se suelta queda en el mundo y es un obstáculo físico: se tropieza con él, se le da una patada,
  rueda.
- Las herramientas son objetos físicos: cada una sirve para una cosa y hay que ir a buscarla.

| Categoría | Ejemplos | Regla |
|---|---|---|
| LIGERO · una mano | Tornillos, herramientas, luces | Se lanzan |
| MEDIO · dos manos | Ruedas, puertas, paragolpes | Más lento, peor visión |
| PESADO · dos jugadores | Motor, caja de cambios | Solo: arrastrar y rezar |

## 7. Los coches

### 7.1 Sistema modular de piezas

Cada coche es un chasis con slots. Cada slot acepta una categoría de pieza.

| Slot | Ejemplos |
|---|---|
| Motor | 3 cilindros, V6, V8 enorme, eléctrico |
| Escape | de serie, deportivo, tubo gigante |
| Ruedas ×4 | normales, pinchadas, todoterreno, gigantes, de carretilla |
| Suspensión ×4 | de serie, rebajada, elevada, rota |
| Carrocería | capó, puertas ×2/×4, maletero, parachoques ×2, alerón |
| Luces | faros, pilotos |
| Cristales | parabrisas, ventanas |
| Interior | asientos, volante, radio |
| Accesorios absurdos | cuernos en el capó, bocina de barco, flotador... |

Cada pieza tiene: condición (0–100), estado (bien / dañada / rota / oxidada / sucia), masa, estadísticas que
afectan a la conducción, y precio.

Referencia: `reference/23_despiece_utilitario_y_montaje.png`.

### 7.2 Montar y desmontar (simple de hacer, físico en las consecuencias)

1. Para quitar una pieza hay que aflojar sus tornillos con la herramienta correcta (mantener botón). Al quitarla,
   cae al suelo como objeto físico.
2. Para poner una pieza se acerca a su hueco y encaja con un imán suave (snap).
3. Después hay que apretar sus tornillos. Si no se aprietan todos, la pieza queda **floja**: aparentemente
   montada, pero puede desprenderse durante la prueba según vibraciones, golpes y velocidad.
4. Una pieza puede montarse al revés si se encaja mal: funciona "a su manera" (luces que apuntan hacia arriba,
   alerón que levanta el coche).

Esta regla (fijación incompleta = consecuencia física diferida) es la fuente principal de "el coche funcionaba
perfectamente... hasta que dejó de funcionar".

### 7.3 Elevar el coche

- **Gato** (inicial): levanta una esquina; si el coche se empuja o se arranca, puede caerse.
- **Borriquetas:** estabilizan, hay que colocarlas.
- **Elevador hidráulico** (mejora): levanta el coche entero; cualquiera puede bajarlo desde el panel, aunque
  haya alguien debajo.
- Un jugador bajo un coche que desciende queda **atrapado** (no muere) hasta que alguien lo levanta.

Referencia: `reference/24_formas_de_elevar.png`.

### 7.4 Diagnóstico y problemas ocultos

Los problemas no siempre se ven. Herramientas para descubrirlos:

- **Escuchar** (estetoscopio de mecánico): acercarse a una zona del motor con él revela ruidos.
- **Escáner** (mejora): muestra códigos de avería.
- **Prueba de conducción:** la forma barata y peligrosa de averiguarlo.

Si no se diagnostica bien, se pagan piezas que no hacían falta.

### 7.5 Personalidad de cada coche

Empezar con 5 modelos base: utilitario diminuto, sedán viejo, furgoneta, pickup, deportivo ochentero.

Cada coche que entra se genera con: modelo + estado de cada pieza + 1–2 rasgos ("el claxon se atasca", "la
puerta del conductor no abre", "huele raro", "tira a la izquierda") + nombre y matrícula.

### 7.6 Modificaciones con consecuencias reales

Las estadísticas del coche se calculan a partir de las piezas reales (masa, par, agarre, altura), no son números
inventados. Por eso las modificaciones absurdas tienen efectos físicos coherentes:

- Motor enorme en coche diminuto → hace caballitos, se va de morro al frenar.
- Ruedas gigantes → centro de masas alto, vuelca en curvas.
- Suspensión rebajada al máximo → roza, saca chispas, se queda enganchado en bordillos.
- Todo encaja en su slot; la "compatibilidad" es física, no un mensaje de error.

## 8. Pintura y limpieza

- Lavado con manguera y esponja (el agua moja el suelo y hace resbalar).
- Lijado para quitar óxido.
- Pintura con pistola: se pinta sobre la superficie con cualquier color. La pintura fresca tarda en secarse; si
  se ensucia o se moja mientras tanto, se estropea.
- Pegatinas y vinilos (mejora).

## 9. Prueba en el exterior

No es un mundo abierto. Es una zona de pruebas compacta y densa pegada al taller: una calle, una cuesta, una
rampa, un camino de tierra, un charco grande, una curva peligrosa, conos, un muro de neumáticos y alguna farola
que se puede derribar. Todo a menos de 30 s del taller.

Durante la prueba pueden pasar cosas por sistema: piezas flojas que se sueltan, sobrecalentamiento → humo →
pequeño fuego (se apaga con extintor), pinchazos, frenos que fallan si estaban mal, averías ocultas que aparecen.

Varios jugadores pueden probar coches a la vez; habrá choques entre ellos.

**Recuperación:** un coche volcado se puede empujar para darle la vuelta; se puede llamar a una grúa (cuesta
dinero). Las piezas perdidas por el mapa se pueden ir a buscar o se recogen al final del día a cambio de una
tasa.

Referencia: `reference/25_plano_zona_pruebas.png`.

## 10. Clientes y compraventa

### 10.1 Encargos

Cada cliente trae un coche y un encargo medible:

| Petición | Cómo se evalúa |
|---|---|
| "Hace un ruido horrible" | La avería que causa el ruido está reparada |
| "Lo necesito en 10 minutos" | Plazo con temporizador visible |
| "Que vaya mucho más rápido" | Velocidad máxima ≥ X (medible en la zona de pruebas) |
| "Que parezca de carreras" | Alerón + escape deportivo + color vivo (checklist de piezas) |
| "Que consuma menos" | Motor de menor consumo / peso total ≤ X |
| "Ruedas enormes" | Tamaño de rueda ≥ X |

- Cada encargo tiene presupuesto, plazo opcional y tolerancia.
- Al entregar se evalúa automáticamente con una checklist visible: pago completo, pago parcial o cliente enfadado.
- Clientes absurdos con peticiones raras pero medibles ("que el claxon suene como un barco", "pintado de 3
  colores", "que salte la rampa").
- Los clientes tienen personalidad visual y reaccionan físicamente al recibir el coche.
- **Reputación:** sube y baja según las entregas; desbloquea clientes mejores y mejor pagados.

Referencia: `reference/26_tarjetas_clientes.png`.

### 10.2 Compraventa

- Comprar coches baratos en un desguace/subasta (con información incompleta del estado).
- Vender coches reparados a compradores con gustos distintos: el precio depende del estado, las piezas y cuánto
  encaja con lo que buscan.

## 11. Economía y progresión

| Nivel del taller | Qué añade |
|---|---|
| 1. Garaje cutre | 1 plaza, gato, llave básica, cubo y esponja |
| 2. Taller de barrio | 2 plazas, borriquetas, elevador, estantería de piezas |
| 3. Nave industrial | 3–4 plazas, grúa de motor, cabina de pintura, escáner |
| 4. Imperio | Concesionario propio, banco de potencia, zona de pruebas ampliada, clientes VIP |

- Las mejoras cambian cómo se juega, no solo números (el elevador cambia el flujo de trabajo; la grúa de motor
  hace posible mover motores en solitario).
- Sumideros de dinero: piezas, coches, herramientas, ampliaciones, grúa, multas, cosméticos.
- Personalización del taller: colocar estanterías, carteles, decoración, color de paredes.

Referencia: `reference/27_progresion_taller.png`.

## 12. Personalización del personaje

Personaje, ropa, casco, guantes, color de herramientas y pegatinas de coches. Solo cosmético, desbloqueado con
dinero del juego y logros absurdos ("se te ha caído un motor encima 10 veces").

## 13. De dónde sale cada momento absurdo

Tabla de control: cada situación deseada debe estar producida por un sistema, no por un guion.

| Situación | Sistema que la genera |
|---|---|
| Dos jugadores colocan una pieza a la vez y se empujan | Snap de piezas + colisión entre personajes |
| Herramienta olvidada bajo un coche | Objetos persistentes + suspensión que aplasta/golpea objetos |
| Arrancar el coche con alguien reparándolo | Cualquiera puede arrancar; motor y ruedas empujan a quien esté cerca |
| Coche sale disparado del taller | Freno de mano quitado + cuesta / mal gato |
| Rueda rodando por la calle | Pieza floja + física de rueda |
| Tropezar cargando un motor | Objetos pesados reducen equilibrio + objetos en el suelo |
| Coche recién pintado en un charco | Pintura con tiempo de secado + agua |
| Pieza montada al revés | Snap con orientación libre |
| Volver de la prueba sin puerta | Pieza floja + vibración/golpes |
| Motor echando humo | Temperatura del motor según piezas y uso |
| Romper el coche del compañero | Daño físico por impacto en piezas |
| Atrapado bajo un coche | Elevador/gato + estado "atrapado" |
| Empujar el coche en grupo y chocar | Fuerza de empuje sumada entre jugadores |
| Reparación simple que empeora todo | Dependencias entre piezas (quitar X suelta Y) |

## 14. Anti-frustración (el caos debe dar risa, no rabia)

- Ningún trabajo individual dura más de 15 minutos: romper algo duele, pero no destroza una tarde.
- Las piezas rotas se pueden vender como chatarra (recuperas algo).
- Siempre hay forma de recuperar objetos perdidos.
- Las consecuencias graves se telegrafían (el coche vibra, la pieza floja traquetea, el motor pita antes del humo).
- Opciones de sala: física "normal" o "caótica", fuego amigo de empujones activado/desactivado.

## 15. Tono

Divertido, relajado, caótico, social, absurdamente físico. Humor visual y de situación. No es un simulador serio:
nada de mecánica realista a nivel de pistón.

## 16. Fuera de alcance para la primera versión

Mundo abierto, conducción simulada realista, PvP competitivo, empleados NPC, microtransacciones, creador de
coches desde cero, modding. Se revisará tras la beta.

## Anexo A · Concepto visual: qué conservar y qué corregir

| Panel | Conservar | Corregir |
|---|---|---|
| `01_portada.jpg` | Logotipo con llave inglesa y engranaje, taller azul con cartel GARAGE, luz de tarde | Los coches recuerdan a modelos reales (pickup tipo americano); deben ser modelos inventados |
| `02_personajes.jpg` | Piel oscura, ojos blancos enormes, boca pequeña, delantales y petos | Proporciones demasiado realistas (cabeza pequeña). La hoja redibujada aplica cabeza ≈ 1/3, manos-manopla y botas grandes |
| `03_taller.jpg` | Desorden legible, elevadores azules, cajas rojas de herramientas, neumáticos apilados. Mejor referencia de ambiente | — |
| `04_coches.jpg` | Variedad de tipos y colores, capós abiertos, óxido | Más rechonchos y "de juguete", ruedas más grandes en proporción |
| `05_objetos.jpg` | Herramientas rojas y azules, cajas, motor, bidones | Exagerar tamaños (llaves 1,2–1,5×, tornillos enormes) |
| `06_reparacion.jpg` | Varios mecánicos sobre el mismo coche, gente tumbada debajo: la fricción cooperativa buscada | — |
| `07_pruebas.jpg` | — | "Competir con amigos" → foco cooperativo; competición solo como minijuego opcional. El paisaje abierto se sustituye por la zona compacta |
| `08_clientes.jpg` | Bocadillos con peticiones | Cada petición debe ser medible |
| `09_mapa_mundo.jpg` | — | Contradice el alcance. Versión 1: taller + zona de pruebas + desguace. El resto, tras la beta |
| `10_eventos_especiales.jpg` | La idea | Incendios, coches que se escapan y grúas que fallan salen de sistemas, no de eventos guionizados |
| `11_personalizacion.jpg` | Personaje, taller y coches. Solo cosmético | — |
| `12_tecnologia.jpg` | — | "Niegara" → Niagara. Lógica en C++, Blueprints solo para datos. Revisar normas de marca de Epic antes de usar el logotipo de Unreal |
| `13_eslogan.jpg` | El eslogan | Rehacer el texto a mano en la versión final (la IA deja errores de tipografía y comillas) |
