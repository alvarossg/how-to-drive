# How to Mechanic — Dirección artística

## 1. Referencia y límites

- Referencia principal de sensación: *How to Fish*. Buscamos el mismo tipo de lenguaje visual: 3D sencillo,
  colorido, caricaturesco, legible y muy físico.
- **No copiamos** sus assets, personajes ni diseños concretos. Tomamos los principios (proporciones, simplicidad,
  color, luz, física) y construimos una identidad propia de taller. Motivos: legal, y para que el juego no
  parezca un clon en Steam.
- Referencias secundarias de física y comedia: *Gang Beasts*, *Human: Fall Flat*, *Totally Reliable Delivery
  Service*, *Overcooked*.
- Carpeta de referencias: `docs/reference/` con capturas propias numeradas y anotadas
  (`01_personaje_frontal.png` + nota de qué mirar). Claude Code puede leer estas imágenes; no puede ver el juego
  por sí solo, así que las especificaciones de este documento mandan.

## 2. Reglas generales

1. **Silueta primero:** todo objeto debe reconocerse solo por su silueta.
2. **Pocas formas, bien exageradas:** herramientas 1,2–1,5× su tamaño real relativo; tornillos y tuercas enormes.
3. **Color plano antes que textura:** sin fotorrealismo, sin ruido de detalle.
4. **Todo lo que se puede separar en el juego es un mesh separado** (puertas, ruedas, capó...). El arte sirve al
   sistema de piezas.

## 3. Modelado

| Categoría | Triángulos orientativos |
|---|---|
| Personaje | 3.000–6.000 |
| Coche completo (todas las piezas) | 5.000–12.000 |
| Pieza de coche | 100–1.500 |
| Herramienta | 100–600 |
| Mobiliario del taller | 200–2.000 |

- Bordes ligeramente biselados, formas redondeadas y rechonchas, sin detalles modelados pequeños.
- Pivotes en el punto lógico de agarre/rotación. Sockets con nombres estandarizados (`SOCKET_Wheel_FL`,
  `SOCKET_Grip`, `SOCKET_Bolt_01`...).
- Colisiones simples (`UCX_`) y Physics Assets con pocos cuerpos.

## 4. Materiales y texturas

- Un material maestro con parámetros (color base, rugosidad, suciedad, óxido, pintura fresca/mojada, daño). Todo
  lo demás son instancias.
- Colores planos o paleta en atlas pequeño (gradientes suaves). Rugosidad alta, casi sin metalicidad salvo
  cromados puntuales.
- Suciedad, óxido y arañazos por máscaras de vértice o de material, porque son estados de juego que cambian
  (lavar, lijar, pintar).
- La pintura del jugador se aplica en una capa de color pintable encima del material base.

**`M_Master`** (un solo material) — parámetros que el código escribe (ver `UHTMVisualLibrary`):

| Parámetro | Tipo | Estado de juego |
|---|---|---|
| `BaseColor` | Vector | Color de fábrica |
| `PaintColor` / `PaintAmount` | Vector / Scalar | Pintura del jugador |
| `Dirt` | Scalar 0–1 | Suciedad (lavar) |
| `Rust` | Scalar 0–1 | Óxido (lijar) |
| `Wetness` / `FreshPaint` | Scalar 0–1 | Mojado / pintura fresca |
| `Damage` | Scalar 0–1 | Abolladura / daño |
| `Highlight` | Scalar 0–1 | Resalte de interacción |

Lavar, lijar y pintar cambian parámetros, no materiales.

## 5. Paleta (tokens iniciales, ajustables)

| Uso | Color |
|---|---|
| Paredes del taller | Verde menta desgastado `#8FC1A9` |
| Suelo | Gris cálido `#B8B0A4` |
| Seguridad / señales | Naranja `#FF8A3D` y amarillo `#FFD23F` |
| Herramientas | Rojo `#E5484D`, azul `#3E7CB1` |
| Óxido | `#B5651D` |
| Coches de serie | Pasteles saturados: celeste `#9ED8F0`, crema `#F6E7C1`, salmón `#F4A08C`, verde lima `#B9E06A` |
| Piel · personaje | `#2F2A38` |
| Interacción / resalte | Blanco con contorno cálido |

Saturación alta pero nunca chillona en grandes superficies; los colores fuertes se reservan para lo interactivo.

En código, la paleta vive en `Source/HowToMechanic/Core/HTMPalette.h` (`HTMPalette::WallMint`, etc.).

## 6. Personajes

- **Proporciones:** cabeza ≈ 1/3 de la altura total; cuerpo en forma de cápsula/pera; brazos largos y flexibles;
  piernas cortas; manos y pies grandes (manos tipo manopla, 3–4 dedos).
- **Cara:** ojos grandes y simples (puntos u óvalos), cejas muy expresivas, boca simple. Expresiones mediante
  texturas intercambiables de ojos y boca (feliz, esfuerzo, susto, KO, enfado), no con rig facial complejo.
- **Ropa:** mono de trabajo, gorra/casco, guantes y botas como piezas intercambiables sobre el mismo esqueleto.
- **Física corporal:** active ragdoll (Physical Animation Component). El cuerpo se tambalea al cargar peso, se
  inclina al empujar, cae como un muñeco.

Reglas de la hoja (`reference/30_hoja_proporciones.png`): manos-manopla grandes · botas grandes = estabilidad ·
ojos blancos enormes · cejas = 80 % de la expresión · brazos largos y blandos · ropa = piezas sobre el mismo
esqueleto · silueta clara a 20 m.

## 7. Animación

- Exagerada, con anticipación y rebote.
- Poses claras de esfuerzo al cargar (piernas abiertas, espalda arqueada).
- Squash & stretch leve en objetos al cogerlos, soltarlos y al impacto.
- Idle con personalidad; celebraciones y frustración en emotes.

## 8. Coches

- Versiones "de juguete": rechonchos, ruedas grandes en proporción, cabinas altas, parachoques gruesos.
- Modelos inventados (no réplicas de marcas reales), aunque recuerden a arquetipos reconocibles.
- Daño visual por estados: abolladura (swap de mesh o morph), cristal roto, pieza colgando.

## 9. Escenarios

- Taller acogedor y desordenado: carteles, manchas de aceite, neumáticos apilados, luces fluorescentes.
- Exterior soleado de barrio industrial con mucho espacio despejado para el caos.
- Cada nivel de taller debe verse claramente más grande, limpio y equipado que el anterior.

## 10. Iluminación y postproceso

- Luz principal cálida y soleada, sombras suaves y definidas, ambiente de cielo relleno.
- Interior del taller: luz cálida de fluorescentes y ventanas.
- Postproceso mínimo: ligera saturación, sin grano, sin viñeta fuerte, bloom muy suave. Prioridad: rendimiento
  estable a 60 fps en PC medio.
- **Decisión pendiente:** si usar contorno (outline) o no. Prueba A/B preparada: consola `htm.Outline 0/1`.

## 11. Efectos (Niagara)

Humo en bolas de dibujo, chispas gordas, estrellitas de KO, "poofs" de polvo, gotas de agua y pintura, fuego
pequeño y estilizado. Pocas partículas, formas grandes.

## 12. Interfaz

- Tipografía redondeada y gruesa, iconos grandes, botones con relieve.
- Diegética siempre que se pueda: pizarra de encargos en la pared, dinero en la caja registradora, precios en
  etiquetas colgando de las piezas.
- Resalte de objetos interactuables con contorno suave y el icono del botón.
- HUD mínimo durante el juego.

## 13. Audio

Sonidos cartoon y exagerados: motores con personalidad, golpes metálicos sonoros, voces tipo murmullo sin idioma
para personajes y clientes, música relajada de radio de taller.

## 14. Pipeline de assets

Blender → FBX/glTF, escala en centímetros, eje Z arriba, nombres con prefijos estándar. Mientras no haya arte
final, placeholders con primitivas y los colores de la paleta, registrados en `docs/ASSET_LIST.md`.

## 15. Checklist de coherencia para cada asset nuevo

- [ ] ¿Se reconoce por la silueta?
- [ ] ¿Está en el rango de triángulos?
- [ ] ¿Usa el material maestro y colores de la paleta?
- [ ] ¿Las piezas separables son meshes separados con sockets correctos?
- [ ] ¿Tiene colisión simple?
- [ ] ¿Encaja en escala junto a un personaje de referencia?
