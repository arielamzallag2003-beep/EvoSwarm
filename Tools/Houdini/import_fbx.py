# Unreal headless import: bring the Houdini FBX meshes into /Game/EvoGen as static meshes.
import unreal
import os

SRC = r"C:\Users\ariel\Documents\Unreal Projects\Evoswarm\Tools\Houdini\out"
DEST = "/Game/EvoGen"

ASSETS = [
    "Evo_Pine_A", "Evo_Pine_B", "Evo_Rock_A", "Evo_Rock_B", "Evo_RockBig",
    "Evo_Bush", "Evo_Cactus", "Evo_Flower", "Evo_Grass", "Evo_Carcass", "Evo_Water",
]

tasks = []
for name in ASSETS:
    fbx = os.path.join(SRC, name + ".fbx")
    if not os.path.exists(fbx):
        unreal.log_warning("MISSING " + fbx)
        continue

    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = False
    opts.import_textures = False
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    opts.static_mesh_import_data.combine_meshes = True
    opts.static_mesh_import_data.generate_lightmap_u_vs = True
    opts.static_mesh_import_data.auto_generate_collision = False
    opts.static_mesh_import_data.remove_degenerates = True
    # Houdini exported in metres; bring them in at centimetre scale (real size in Unreal).
    opts.static_mesh_import_data.import_uniform_scale = 100.0

    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = opts
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# Report what landed and the bounds (so we can size the scatter correctly).
for name in ASSETS:
    path = DEST + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        try:
            b = mesh.get_bounding_box()
            size = b.max - b.min
            unreal.log("IMPORTED " + name + " size_cm=({:.1f},{:.1f},{:.1f})".format(size.x, size.y, size.z))
        except Exception as e:
            unreal.log("IMPORTED " + name + " (no bounds) " + str(e))
    else:
        unreal.log_warning("FAILED " + name)

unreal.EditorAssetLibrary.save_directory(DEST)
print("IMPORT_DONE")
