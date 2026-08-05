# Discover exact parameter names for the SOP nodes used by the asset generator.
import hou

geo = hou.node("/obj").createNode("geo", "discover")
for c in list(geo.children()):
    try:
        c.destroy()
    except Exception:
        pass

for ntype in ("tube", "grid", "sphere", "mountain", "facet", "box", "xform", "merge", "normal"):
    try:
        n = geo.createNode(ntype)
        names = [pt.name() for pt in n.parmTemplateGroup().entriesWithoutFolders()]
        print(ntype, "->", names)
    except Exception as e:
        print(ntype, "ERR", repr(e))
print("DONE")
