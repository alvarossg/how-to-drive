"""Crea los materiales por script (docs/ART_DIRECTION.md §4 y §10):

  /Game/Materials/M_Master      Material maestro de todo el mundo. Parámetros:
      BaseColor, PaintColor (vector) · PaintAmount, Dirt, Rust, Wetness, FreshPaint, Damage, Highlight (escalar)
      Estados de juego -> aspecto: pintura sobre base, óxido y suciedad encima, mojado oscurece y da brillo,
      pintura fresca brilla, daño oscurece, Highlight = resalte naranja suave (sin contorno).
  /Game/Materials/M_PP_Outline  Post-proceso de contorno para la prueba A/B (consola htm.Outline 0/1):
      línea de tinta por discontinuidad de profundidad y halo naranja en lo que tiene stencil 1
      (el objeto enfocado; r.CustomDepth=3 en DefaultEngine.ini).

Uso:  py Tools/Scripts/create_master_material.py   (vuelve a generarlos si ya existen)
"""

import unreal

import htm_editor_utils as htm

MEL = unreal.MaterialEditingLibrary
PACKAGE = "/Game/Materials"


def _new_material(name):
    path = "{}/{}".format(PACKAGE, name)
    if htm.asset_exists(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = htm.asset_tools().create_asset(name, PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        htm.error("No se pudo crear " + path)
    return material


def _expr(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def _vector_param(material, name, color, x, y):
    node = _expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", color)
    return node


def _scalar_param(material, name, value, x, y):
    node = _expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def _constant3(material, color, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", color)
    return node


def _lerp(material, a, b, alpha, x, y):
    node = _expr(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, "", node, "A")
    MEL.connect_material_expressions(b, "", node, "B")
    MEL.connect_material_expressions(alpha, "", node, "Alpha")
    return node


def _multiply(material, a, b, x, y):
    node = _expr(material, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, "", node, "A")
    MEL.connect_material_expressions(b, "", node, "B")
    return node


def _scaled(material, source, factor, x, y):
    const = _expr(material, unreal.MaterialExpressionConstant, x - 150, y + 40)
    const.set_editor_property("r", factor)
    return _multiply(material, source, const, x, y)


def create_master():
    m = _new_material("M_Master")
    if m is None:
        return False
    m.set_editor_property("two_sided", False)

    base = _vector_param(m, "BaseColor", htm.hex_to_linear("B8B0A4"), -1400, -400)
    paint = _vector_param(m, "PaintColor", htm.hex_to_linear("FAF7F0"), -1400, -200)
    paint_amount = _scalar_param(m, "PaintAmount", 0.0, -1400, 0)
    rust = _scalar_param(m, "Rust", 0.0, -1400, 100)
    dirt = _scalar_param(m, "Dirt", 0.0, -1400, 200)
    wet = _scalar_param(m, "Wetness", 0.0, -1400, 300)
    fresh = _scalar_param(m, "FreshPaint", 0.0, -1400, 400)
    damage = _scalar_param(m, "Damage", 0.0, -1400, 500)
    highlight = _scalar_param(m, "Highlight", 0.0, -1400, 600)

    # Color: base -> pintura -> óxido -> suciedad -> daño -> mojado.
    painted = _lerp(m, base, paint, paint_amount, -1100, -300)
    rust_color = _constant3(m, htm.hex_to_linear(htm.PALETTE["Rust"]), -1100, -100)
    rusty = _lerp(m, painted, rust_color, _scaled(m, rust, 0.75, -1100, 60), -900, -250)
    dirt_linear = htm.hex_to_linear(htm.PALETTE["Dirt"])
    dirt_color = _constant3(m, unreal.LinearColor(dirt_linear.r * 0.6, dirt_linear.g * 0.6, dirt_linear.b * 0.6, 1.0), -900, -50)
    dirty = _lerp(m, rusty, dirt_color, _scaled(m, dirt, 0.6, -900, 120), -700, -200)
    damage_color = _constant3(m, htm.hex_to_linear(htm.PALETTE["Ink"]), -700, 0)
    damaged = _lerp(m, dirty, damage_color, _scaled(m, damage, 0.45, -700, 150), -500, -150)
    one = _expr(m, unreal.MaterialExpressionConstant, -500, 100)
    one.set_editor_property("r", 1.0)
    wet_dark = _lerp(m, one, _scaled(m, one, 0.75, -500, 200), wet, -350, 60)
    final_color = _multiply(m, damaged, wet_dark, -200, -100)
    MEL.connect_material_property(final_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Rugosidad: mate por defecto; mojado y pintura fresca brillan.
    rough_matte = _expr(m, unreal.MaterialExpressionConstant, -700, 400)
    rough_matte.set_editor_property("r", 0.85)
    rough_gloss = _expr(m, unreal.MaterialExpressionConstant, -700, 480)
    rough_gloss.set_editor_property("r", 0.2)
    shiny = _expr(m, unreal.MaterialExpressionMax, -700, 560)
    MEL.connect_material_expressions(wet, "", shiny, "A")
    MEL.connect_material_expressions(fresh, "", shiny, "B")
    roughness = _lerp(m, rough_matte, rough_gloss, shiny, -450, 450)
    MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Resalte (sin contorno): emisivo naranja suave.
    orange = _constant3(m, htm.hex_to_linear(htm.PALETTE["SafetyOrange"]), -700, 700)
    glow = _multiply(m, orange, _scaled(m, highlight, 0.35, -500, 760), -300, 700)
    MEL.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(m)
    htm.save(m)
    htm.log("M_Master creado")
    return True


OUTLINE_HLSL = r"""
float2 Texel = View.BufferSizeAndInvSize.zw * Thickness;
float D  = SceneTextureLookup(UV, 1, false).r;
float DL = SceneTextureLookup(UV + float2(-Texel.x, 0), 1, false).r;
float DR = SceneTextureLookup(UV + float2( Texel.x, 0), 1, false).r;
float DU = SceneTextureLookup(UV + float2(0, -Texel.y), 1, false).r;
float DD = SceneTextureLookup(UV + float2(0,  Texel.y), 1, false).r;
float Edge = saturate((abs(DL - D) + abs(DR - D) + abs(DU - D) + abs(DD - D)) / max(D, 1.0) * Sensitivity);
Edge *= saturate(1.0 - D / FadeDistance);

float S  = SceneTextureLookup(UV, 25, false).r;
float SN = max(max(SceneTextureLookup(UV + float2(-Texel.x * 2, 0), 25, false).r,
                   SceneTextureLookup(UV + float2( Texel.x * 2, 0), 25, false).r),
               max(SceneTextureLookup(UV + float2(0, -Texel.y * 2), 25, false).r,
                   SceneTextureLookup(UV + float2(0,  Texel.y * 2), 25, false).r));
float Halo = (SN > 0.5 && S < 0.5) ? 1.0 : 0.0;

float3 Col = lerp(SceneColor, InkColor, Edge);
return lerp(Col, HighlightColor, Halo);
"""


def create_outline():
    m = _new_material("M_PP_Outline")
    if m is None:
        return False
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    for location in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
        if hasattr(unreal.BlendableLocation, location):
            m.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, location))
            break

    scene = _expr(m, unreal.MaterialExpressionSceneTexture, -900, -200)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    # El nodo SceneTexture conectado habilita SceneTextureLookup() en el HLSL (1 = SceneDepth, 25 = CustomStencil).
    uv = _expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)

    thickness = _scalar_param(m, "Thickness", 1.5, -900, 300)
    sensitivity = _scalar_param(m, "Sensitivity", 8.0, -900, 400)
    fade = _scalar_param(m, "FadeDistance", 6000.0, -900, 500)
    ink = _vector_param(m, "InkColor", htm.hex_to_linear(htm.PALETTE["Ink"]), -900, 600)
    hl = _vector_param(m, "HighlightColor", htm.hex_to_linear(htm.PALETTE["SafetyOrange"]), -900, 800)

    custom = _expr(m, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", OUTLINE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "HTM Outline")
    names = ["SceneColor", "UV", "Thickness", "Sensitivity", "FadeDistance", "InkColor", "HighlightColor"]
    inputs = []
    for name in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    MEL.connect_material_expressions(scene, "Color", custom, "SceneColor")
    MEL.connect_material_expressions(uv, "", custom, "UV")
    MEL.connect_material_expressions(thickness, "", custom, "Thickness")
    MEL.connect_material_expressions(sensitivity, "", custom, "Sensitivity")
    MEL.connect_material_expressions(fade, "", custom, "FadeDistance")
    MEL.connect_material_expressions(ink, "", custom, "InkColor")
    MEL.connect_material_expressions(hl, "", custom, "HighlightColor")
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(m)
    htm.save(m)
    htm.log("M_PP_Outline creado")
    return True


def main():
    htm.ensure_directory(PACKAGE)
    return create_master() and create_outline()


if __name__ == "__main__":
    main()
