# Create a translucent, glossy water material (M_Water) so the sea reads as water, not flat paint.
import unreal

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/EvoGen"
NAME = "M_Water"
FULL = PATH + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(FULL):
    unreal.EditorAssetLibrary.delete_asset(FULL)

mat = TOOLS.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
try:
    mat.set_editor_property("translucency_lighting_mode",
                            unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
except Exception as e:
    unreal.log_warning("tlm_err " + str(e))

# Deep blue base colour.
col = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, -120)
col.set_editor_property("constant", unreal.LinearColor(0.05, 0.27, 0.42, 1.0))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

# Strong specular + low roughness -> shiny, water-like sun glints on the facets.
spec = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 40)
spec.set_editor_property("r", 1.0)
MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 140)
rough.set_editor_property("r", 0.06)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

# Opacity: fresnel makes it see-through looking down, more opaque at grazing angles.
opacity_done = False
try:
    fres = MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel, -850, 280)
    fres.set_editor_property("exponent", 4.0)
    fres.set_editor_property("base_reflect_fraction", 0.04)
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 280)
    MEL.connect_material_expressions(fres, "", mul, "A")
    mul.set_editor_property("const_b", 0.5)
    add = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, -480, 280)
    MEL.connect_material_expressions(mul, "", add, "A")
    add.set_editor_property("const_b", 0.42)
    MEL.connect_material_property(add, "", unreal.MaterialProperty.MP_OPACITY)
    opacity_done = True
except Exception as e:
    unreal.log_warning("fresnel_err " + str(e))

if not opacity_done:
    op = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 280)
    op.set_editor_property("r", 0.55)
    MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(FULL)
print("WATER_MAT_DONE exists=" + str(unreal.EditorAssetLibrary.does_asset_exist(FULL)))
