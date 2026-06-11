# Procedurally build a set of stylized low-poly meshes and export each as FBX.
# Built Z-up to match Unreal; flat-shaded (faceted) for the low-poly look.
import hou, os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)

ROOT = hou.node("/obj")
ROP = hou.node("/out").createNode("filmboxfbx")


def setp(node, name, value):
    p = node.parm(name)
    if p is not None:
        try:
            p.set(value)
            return True
        except Exception:
            pass
    return False


def sett(node, name, values):
    pt = node.parmTuple(name)
    if pt is not None:
        try:
            pt.set(values)
            return True
        except Exception:
            pass
    return False


def newgeo(name):
    g = ROOT.createNode("geo", name)
    for c in list(g.children()):
        try:
            c.destroy()
        except Exception:
            pass
    return g


def orient_z(node):
    # Try to orient a tube/grid so its axis/up is +Z.
    if not setp(node, "orient", "z"):
        setp(node, "orient", 2)


def cone(geo, base_r, top_r, height, zbase, cols=7):
    t = geo.createNode("tube")
    setp(t, "type", "poly")
    orient_z(t)
    setp(t, "cap", 1)
    sett(t, "rad", (base_r, top_r))
    setp(t, "height", height)
    setp(t, "cols", cols)
    setp(t, "rows", 1)
    x = geo.createNode("xform")
    x.setInput(0, t)
    sett(x, "t", (0.0, 0.0, zbase + height * 0.5))
    return x


def sphere(geo, rad, rows=8, cols=10):
    s = geo.createNode("sphere")
    setp(s, "type", "poly")
    sett(s, "rad", (rad, rad, rad))
    setp(s, "rows", rows)
    setp(s, "cols", cols)
    return s


def mountain(geo, node, height, esize=1.0):
    m = geo.createNode("mountain")
    m.setInput(0, node)
    setp(m, "height", height)
    setp(m, "elementsize", esize)
    return m


def facet_flat(geo, node, angle=18.0):
    f = geo.createNode("facet")
    f.setInput(0, node)
    setp(f, "unique", 1)
    setp(f, "cusp", 1)
    setp(f, "angle", angle)
    return f


def xform(geo, node, t=(0, 0, 0), r=(0, 0, 0), s=(1, 1, 1)):
    x = geo.createNode("xform")
    x.setInput(0, node)
    sett(x, "t", t)
    sett(x, "r", r)
    sett(x, "s", s)
    return x


def merge(geo, nodes):
    m = geo.createNode("merge")
    for i, n in enumerate(nodes):
        m.setInput(i, n)
    return m


def export(geo, last, name):
    last.setDisplayFlag(True)
    last.setRenderFlag(True)
    path = os.path.join(OUT, name + ".fbx")
    setp(ROP, "startnode", geo.path())
    setp(ROP, "sopoutput", path)
    ROP.render()
    g = last.geometry()
    print("EXPORT", name, "pts", len(g.points()), "prims", len(g.prims()), "ok", os.path.exists(path))


# --- Pine A: 3 stacked cones + tiny trunk ---
g = newgeo("Evo_Pine_A")
trunk = cone(g, 0.07, 0.07, 0.28, 0.0, cols=6)
t1 = cone(g, 0.55, 0.0, 0.55, 0.18, cols=8)
t2 = cone(g, 0.42, 0.0, 0.55, 0.52, cols=8)
t3 = cone(g, 0.28, 0.0, 0.50, 0.88, cols=8)
export(g, facet_flat(g, merge(g, [trunk, t1, t2, t3])), "Evo_Pine_A")

# --- Pine B: taller, 4 narrower cones ---
g = newgeo("Evo_Pine_B")
trunk = cone(g, 0.06, 0.06, 0.3, 0.0, cols=6)
cs = [cone(g, 0.46 - i * 0.09, 0.0, 0.5, 0.2 + i * 0.36, cols=7) for i in range(4)]
export(g, facet_flat(g, merge(g, [trunk] + cs)), "Evo_Pine_B")

# --- Rock A: faceted boulder ---
g = newgeo("Evo_Rock_A")
export(g, facet_flat(g, mountain(g, sphere(g, 0.6, 8, 10), 0.32, 0.8), 26), "Evo_Rock_A")

# --- Rock B: angular box rock ---
g = newgeo("Evo_Rock_B")
b = g.createNode("box")
sett(b, "scale", (0.7, 0.55, 0.5))
export(g, facet_flat(g, mountain(g, b, 0.28, 0.7), 30), "Evo_Rock_B")

# --- Rock Big: large jagged formation (terrain detail) ---
g = newgeo("Evo_RockBig")
export(g, facet_flat(g, mountain(g, sphere(g, 1.0, 9, 11), 0.7, 0.6), 30), "Evo_RockBig")

# --- Bush: clumped noised spheres ---
g = newgeo("Evo_Bush")
b1 = sphere(g, 0.5, 7, 9)
b2 = xform(g, sphere(g, 0.38, 6, 8), t=(0.3, 0.15, 0.1))
b3 = xform(g, sphere(g, 0.34, 6, 8), t=(-0.25, -0.2, 0.05))
export(g, facet_flat(g, mountain(g, merge(g, [b1, b2, b3]), 0.18, 0.5), 22), "Evo_Bush")

# --- Cactus: body + two arms ---
g = newgeo("Evo_Cactus")
body = cone(g, 0.18, 0.16, 1.0, 0.0, cols=8)
armL = xform(g, cone(g, 0.08, 0.07, 0.4, 0.0, cols=6), r=(0, 40, 0), t=(-0.22, 0, 0.5))
armR = xform(g, cone(g, 0.08, 0.07, 0.4, 0.0, cols=6), r=(0, -40, 0), t=(0.22, 0, 0.62))
export(g, facet_flat(g, merge(g, [body, armL, armR]), 30), "Evo_Cactus")

# --- Flower: thin stem + small bloom ---
g = newgeo("Evo_Flower")
stem = cone(g, 0.03, 0.03, 0.35, 0.0, cols=5)
bloom = xform(g, sphere(g, 0.13, 5, 7), t=(0, 0, 0.4), s=(1, 1, 0.6))
export(g, facet_flat(g, merge(g, [stem, bloom]), 30), "Evo_Flower")

# --- Grass tuft (food plant): a few tilted blades ---
g = newgeo("Evo_Grass")
blades = []
for i, (rr, tt) in enumerate(((0, (0.0, 0.0)), (1, (0.12, 0.05)), (2, (-0.1, 0.08)), (3, (0.04, -0.12)))):
    bl = cone(g, 0.05, 0.0, 0.4 + 0.08 * i % 0.2, 0.0, cols=4)
    bl = xform(g, bl, r=(rr * 8 - 12, 0, i * 40), t=(tt[0], tt[1], 0))
    blades.append(bl)
export(g, facet_flat(g, merge(g, blades), 40), "Evo_Grass")

# --- Carcass: squashed faceted lump ---
g = newgeo("Evo_Carcass")
lump = xform(g, sphere(g, 0.5, 7, 9), s=(1.2, 0.8, 0.45))
export(g, facet_flat(g, mountain(g, lump, 0.12, 0.6), 28), "Evo_Carcass")

# --- Water: low-poly faceted plane with gentle ripples ---
g = newgeo("Evo_Water")
grid = g.createNode("grid")
sett(grid, "size", (2.0, 2.0))
setp(grid, "rows", 24)
setp(grid, "cols", 24)
if not setp(grid, "orient", "xy"):
    setp(grid, "orient", 0)
export(g, facet_flat(g, mountain(g, grid, 0.04, 0.25), 8), "Evo_Water")

print("ALL_DONE")
