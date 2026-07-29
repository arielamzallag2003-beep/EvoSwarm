# Builds /Game/EvoGen/M_Terrain: an opaque material that routes mesh VERTEX COLOUR to base
# colour (plus full roughness), used by the procedural terrain so its smoothly blended biome
# tints are visible. Companion to make_water_mat.py — run headless with:
#   UnrealEditor-Cmd.exe <project.uproject> -run=pythonscript -script="Tools/Houdini/make_terrain_mat.py"
import unreal

PKG_PATH = "/Game/EvoGen"
NAME = "M_Terrain"
FULL = PKG_PATH + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(FULL):
    unreal.log("{} already exists; leaving it untouched.".format(FULL))
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(NAME, PKG_PATH, unreal.Material, unreal.MaterialFactoryNew())

    vc = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVertexColor, -350, 0)
    unreal.MaterialEditingLibrary.connect_material_property(
        vc, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant, -350, 220)
    rough.set_editor_property("r", 0.9)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(mat)
    ok = unreal.EditorAssetLibrary.save_asset(FULL, only_if_is_dirty=False)
    unreal.log("MADE {} saved={}".format(FULL, ok))
