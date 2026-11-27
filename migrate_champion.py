import os
import shutil
import struct
import numpy as np

CHAMPION = "bin/data/Config/ai_value_net.bin"
BACKUP = "bin/data/Config/ai_value_net_gen60_backup.bin"

MAGIC = 0x4D465356
W, H = 13, 9
OLD_GC, OLD_GDIM = 42, 590
NEW_GC, NEW_GDIM = 48, 947
CONV_CH, REDUCE_CH, FC1_DIM, FC2_DIM = 16, 4, 128, 64

OLD_MLP_IN = (W * H * REDUCE_CH) + (CONV_CH * 2) + OLD_GDIM  # 1090
NEW_MLP_IN = (W * H * REDUCE_CH) + (CONV_CH * 2) + NEW_GDIM  # 1447

with open(CHAMPION, "rb") as f:
    hdr = f.read(36)
    vals = struct.unpack("<Iiiiiiiii", hdr)
    assert vals == (MAGIC, W, H, OLD_GC, OLD_GDIM, CONV_CH, REDUCE_CH, FC1_DIM, FC2_DIM), f"Unexpected header: {vals}"
    blob = np.frombuffer(f.read(), dtype=np.float32).copy()

shutil.copy2(CHAMPION, BACKUP)

p = 0
def take(n):
    global p
    out = blob[p : p + n]
    p += n
    return out

old_proj_w = take(CONV_CH * OLD_GC).reshape(CONV_CH, 1, 1, OLD_GC)
new_proj_w = np.zeros((CONV_CH, 1, 1, NEW_GC), dtype=np.float32)
new_proj_w[:, :, :, :OLD_GC] = old_proj_w

mid_weights = take(
    CONV_CH * 3                     # proj_b, ln_proj_w, ln_proj_b
    + (CONV_CH * 9 * CONV_CH + CONV_CH * 3) * 2  # conv1 + conv2 blocks
    + (REDUCE_CH * CONV_CH + REDUCE_CH)          # conv_red
)

old_fc1_w = take(FC1_DIM * OLD_MLP_IN).reshape(FC1_DIM, OLD_MLP_IN)
new_fc1_w = np.zeros((FC1_DIM, NEW_MLP_IN), dtype=np.float32)
new_fc1_w[:, :OLD_MLP_IN] = old_fc1_w

tail_weights = take(
    FC1_DIM * 3                     # fc1_b, ln_fc1_w, ln_fc1_b
    + FC2_DIM * FC1_DIM + FC2_DIM * 3            # fc2 block
    + FC2_DIM + 1                                # out
)

assert p == len(blob), f"Length mismatch: consumed {p}, total {len(blob)}"

with open(CHAMPION, "wb") as f:
    f.write(struct.pack("<Iiiiiiiii", MAGIC, W, H, NEW_GC, NEW_GDIM, CONV_CH, REDUCE_CH, FC1_DIM, FC2_DIM))
    f.write(new_proj_w.tobytes())
    f.write(mid_weights.tobytes())
    f.write(new_fc1_w.tobytes())
    f.write(tail_weights.tobytes())

print(f"Backed up Gen-60 champion to {BACKUP} and migrated {CHAMPION} to (48, 947)!")