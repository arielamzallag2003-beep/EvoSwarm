import unreal

DEST = "/Game/EvoGen"
NAMES = ["Evo_Pine_A", "Evo_Pine_B", "Evo_Rock_A", "Evo_Rock_B", "Evo_RockBig",
         "Evo_Bush", "Evo_Cactus", "Evo_Flower", "Evo_Grass", "Evo_Carcass", "Evo_Water"]

for name in NAMES:
    p = DEST + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(p):
        unreal.log_warning("BND_MISSING " + name)
        continue
    m = unreal.EditorAssetLibrary.load_asset(p)
    b = m.get_bounds()
    e = b.box_extent
    o = b.origin
    unreal.log_warning("BND {} full=({:.1f},{:.1f},{:.1f}) origin=({:.1f},{:.1f},{:.1f})".format(
        name, e.x * 2, e.y * 2, e.z * 2, o.x, o.y, o.z))

print("BOUNDS_DONE")
