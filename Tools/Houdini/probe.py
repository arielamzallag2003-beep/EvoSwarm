# Houdini hython probe: confirm node-type names + which mesh export format works.
import hou, os, traceback

print("VERSION", hou.applicationVersionString())

sopcat = hou.sopNodeTypeCategory()
names = set(sopcat.nodeTypes().keys())
wanted = ["sphere", "box", "tube", "grid", "line", "mountain", "facet", "scatter",
          "copytopoints", "copytopoints::2.0", "polyextrude", "polyextrude::2.0",
          "subdivide", "normal", "merge", "xform", "polyreduce::2.0", "attribnoise",
          "color", "attribrandomize", "polybevel", "polybevel::3.0"]
print("HAVE", {w: (w in names) for w in wanted})

outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(outdir, exist_ok=True)

obj = hou.node("/obj")
geo = obj.createNode("geo", "rocktest")
for c in list(geo.children()):
    try:
        c.destroy()
    except Exception:
        pass

sph = geo.createNode("sphere")
# Make it a low-poly polygon sphere.
try:
    sph.parm("type").set("poly")
except Exception:
    try:
        sph.parm("type").set(1)
    except Exception as e:
        print("sphere_type_err", e)
for pn, val in (("rows", 9), ("cols", 12), ("orderu", 0), ("orderv", 0)):
    p = sph.parm(pn)
    if p:
        try:
            p.set(val)
        except Exception:
            pass

mtn = geo.createNode("mountain")
mtn.setInput(0, sph)
p = mtn.parm("height")
if p:
    p.set(0.3)

facet = geo.createNode("facet")
facet.setInput(0, mtn)
for pn, val in (("unique", True), ("cusp", True), ("cuspangle", 25)):
    p = facet.parm(pn)
    if p:
        try:
            p.set(val)
        except Exception:
            pass
facet.setDisplayFlag(True)
facet.setRenderFlag(True)

g = facet.geometry()
print("GEO_POINTS", len(g.points()), "GEO_PRIMS", len(g.prims()))

obj_path = os.path.join(outdir, "rocktest.obj")
try:
    g.saveToFile(obj_path)
    print("OBJ_SAVED", os.path.exists(obj_path), obj_path)
except Exception as e:
    print("OBJ_ERR", repr(e))

fbx_path = os.path.join(outdir, "rocktest.fbx")
try:
    ropnet = hou.node("/out")
    fbx = ropnet.createNode("filmboxfbx")
    if fbx.parm("startnode"):
        fbx.parm("startnode").set(geo.path())
    file_param = None
    for pn in ("sopoutput", "outfile", "file", "fbxfile"):
        if fbx.parm(pn):
            file_param = pn
            break
    if file_param is None:
        print("FBX_FILE_PARAMS", [p.name() for p in fbx.parms() if "file" in p.name().lower() or "output" in p.name().lower()])
    else:
        fbx.parm(file_param).set(fbx_path)
        fbx.render()
        print("FBX_SAVED", os.path.exists(fbx_path), "param", file_param)
except Exception as e:
    print("FBX_ERR", repr(e))

print("DONE")
