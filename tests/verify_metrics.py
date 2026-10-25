"""Independent verification of safe-sensor-ecu safety claims.
Reimplements the EXACT Rust algorithm (integer) and cross-checks vs. committed weights.
"""
import re, sys, json
import numpy as np
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "sim"))

# ---- Parse committed Rust weight table (authoritative artifact) ----
txt = (ROOT / "swc_rust_app" / "src" / "weights_generated.rs").read_text()

def grab(name, dims):
    i = txt.index("pub static " + name)
    j = txt.index("= [", i)
    # balance brackets from the first '[' after '='
    k = j + 1
    depth = 0
    while True:
        if txt[k] == "[":
            depth += 1
        elif txt[k] == "]":
            depth -= 1
            if depth == 0:
                break
        k += 1
    body = txt[j+1:k]
    body = re.sub(r"//.*", "", body)
    nums = [int(x) for x in re.findall(r"-?\d+", body)]
    return np.array(nums, dtype=np.int64).reshape(dims)

W1 = grab("W1_Q13", (32, 6))
B1 = grab("B1_Q13", (32,))
W2 = grab("W2_Q13", (3, 32))
B2 = grab("B2_Q13", (3,))
print(f"Parsed committed weights: W1{W1.shape} B1{B1.shape} W2{W2.shape} B2{B2.shape}")
print(f"  B1 range [{B1.min()},{B1.max()}]  B2 range [{B2.min()},{B2.max()}]")

SHIFT = 13

def clamp_i16(v):
    return max(-32768, min(32767, v))

def rust_fnn(x):
    """Exact port of run_q13_fnn_filter in swc_rust_app/src/lib.rs"""
    h = np.zeros(32, dtype=np.int64)
    for j in range(32):
        acc = int(B1[j])
        for i in range(6):
            acc += int(W1[j][i]) * int(x[i])
        shifted = acc >> SHIFT          # Rust i64 >> is arithmetic (floor) shift
        h[j] = shifted if shifted > 0 else 0
    out = np.zeros(3, dtype=np.int64)
    for k in range(3):
        acc2 = int(B2[k])
        for m in range(32):
            acc2 += int(W2[k][m]) * int(h[m])
        out[k] = clamp_i16(acc2 >> SHIFT)
    return out

MAX_ASYM = 4800
MAX_CERT = 3800
SWITCH_THRESH = 1100

def rust_plausibility(x, y_pred):
    dx = abs(int(x[0]) - int(x[3])); dy = abs(int(x[1]) - int(x[4])); dz = abs(int(x[2]) - int(x[5]))
    l1 = dx + dy + dz
    corr_bz = abs(int(x[2]) - int(y_pred[2]))
    plausible = (l1 <= MAX_ASYM) and (corr_bz <= MAX_CERT)
    witness = abs(int(x[3])) + abs(int(x[4])) + abs(int(x[5]))
    return plausible, witness, l1, corr_bz

# ---- Load the training pipeline for reference data ----
import importlib.util
spec = importlib.util.spec_from_file_location("tq", ROOT / "sim" / "train_and_quantize.py")
tq = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tq)

x_all, y_all, ext_all = tq.generate_dataset(26000)
x_test, y_test = x_all[21000:], y_all[21000:]

# ================= CHECK 1: Rust vs Python-reference bit parity =================
print("\n=== CHECK 1: Rust integer FNN vs Python reference (committed weights) ===")
W1q = W1; b1q = B1; W2q = W2; b2q = B2
xi = np.round(x_test).astype(np.int64)
z1i = (xi @ W1q.T + b1q[None, :]) >> SHIFT
h1i = np.maximum(0, z1i)
q_ref = ((h1i @ W2q.T + b2q[None, :]) >> SHIFT)

rust_out = np.array([rust_fnn(r) for r in xi])
# python ref is unclamped; rust clamps to i16
diff = np.abs(rust_out - q_ref)
print(f"  max |rust - pyref| over 5000 test samples = {diff.max()}")
print(f"  samples differing (due to i16 clamp)     = {(diff.max(axis=1) > 0).sum()}")
print(f"  max unclamped pyref value                 = {q_ref.max()}  (i16 max = 32767)")

# ================= CHECK 2: 1oo2 plausibility monitor FALSE POSITIVES ==========
print("\n=== CHECK 2: 1oo2 plausibility monitor on LEGITIMATE (non-attack) test data ===")
fp = 0; ood_rows = []
plaus = []
for i in range(len(xi)):
    p, w, l1, corr = rust_plausibility(xi[i], rust_out[i])
    plaus.append(p)
    if not p:
        fp += 1
        ood_rows.append((i, l1, corr, float(np.abs(ext_all[21000+i]).max()) if ext_all is not None else 0))
plaus = np.array(plaus)
print(f"  LEGITIMATE samples flagged OOD (false positive) = {fp}/{len(xi)} = {100.0*fp/len(xi):.2f}%")
if ood_rows:
    print(f"  example OOD rows (idx, l1_grad, corr_bz, max|ext| mT): {ood_rows[:6]}")
    l1s = [r[1] for r in ood_rows]; corrs = [r[2] for r in ood_rows]
    print(f"  among FPs: l1_grad max={max(l1s)} min={min(l1s)} | corr_bz max={max(corrs)} min={min(corrs)}")

# ================= CHECK 3: end-to-end deployed decision path =================
print("\n=== CHECK 3: FULL deployed decision path (monitor + 1-pole filter + threshold) ===")
prev = 0
sw_states = []
for i in range(len(xi)):
    y = rust_fnn(xi[i])
    p, w, l1, corr = rust_plausibility(xi[i], y)
    if not p:
        sw_states.append(0)      # safe state
        continue
    filt = (3*int(y[2]) + prev) >> 2
    prev = filt
    sw_states.append(1 if filt >= SWITCH_THRESH else 0)
sw_states = np.array(sw_states)
true_sw = (y_test[:, 2] >= 1100.0).astype(int)
# network-only decision (what training_metrics reports)
net_only = (q_ref[:, 2] >= 1100.0).astype(int)
print(f"  training_metrics reported false-switch rate (network only) = {100*np.mean(net_only != true_sw):.2f}%")
print(f"  DEPLOYED full-path false-switch rate (monitor+filter)       = {100*np.mean(sw_states != true_sw):.2f}%")
print(f"  deployed FALSE-NEGATIVE (missed real press) = {100*np.mean((sw_states==0)&(true_sw==1)):.2f}%")
print(f"  deployed FALSE-POSITIVE (spurious press)   = {100*np.mean((sw_states==1)&(true_sw==0)):.2f}%")

# ================= CHECK 4: IBP soundness =================
print("\n=== CHECK 4: IBP soundness - does claimed bound contain true outputs? ===")
ibp = json.loads((ROOT / "sim" / "training_metrics.json").read_text())["ibp_output_bounds_centimt"]
lo = np.array(ibp["low"]); hi = np.array(ibp["high"])
print(f"  claimed IBP low  = {lo.tolist()}")
print(f"  claimed IBP high = {hi.tolist()}")
rng = np.random.default_rng(7)
viol = 0; tested = 0
X = rng.integers(-13000, 13001, size=(4000, 6))
for x in X:
    o = rust_fnn(x)
    tested += 1
    if np.any(o < lo) or np.any(o > hi):
        viol += 1
print(f"  random points in certified box violating claimed IBP: {viol}/{tested}")
# exhaustive corner check
import itertools
corners = []
for s in itertools.product([-13000, 13000], repeat=6):
    corners.append(s)
cv = 0
for c in corners:
    o = rust_fnn(c)
    if np.any(o < lo) or np.any(o > hi):
        cv += 1
print(f"  all 64 box corners violating claimed IBP: {cv}/64")
print(f"  NOTE: actual Rust output is clamped to i16 -> true output range is [-32768, 32767]")

# ================= CHECK 5: does IBP bound reflect the DEPLOYED clamp? ======
print("\n=== CHECK 5: IBP vs actual i16-clamped deployment ===")
print(f"  claimed IBP width (high-low) = {(hi-lo).tolist()}")
print(f"  i16 representable output range = [-32768, 32767]")
print(f"  IBP is vacuous vs deployment: bounds exceed i16 range by "
      f"{max(hi.max(), -lo.min())/32767.0:.1f}x")

# ================= CHECK 6: spoofing attack detection at 500/500 claimed ======
print("\n=== CHECK 6: Reproduce HIL spoofing vector through Rust pipeline ===")
sp = np.array([4200, -2900, 6800, 610, -300, 920], dtype=np.int64)
y = rust_fnn(sp)
p, w, l1, corr = rust_plausibility(sp, y)
print(f"  HIL attack vector -> y_pred={y.tolist()}")
print(f"  l1_grad={l1} (limit {MAX_ASYM}), corr_bz={corr} (limit {MAX_CERT})")
print(f"  plausible={p} -> {'DETECTED' if not p else '*** MISSED ***'}")

