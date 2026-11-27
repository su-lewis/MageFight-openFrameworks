import glob
import os
import struct
import numpy as np
import torch
import torch.nn as nn

GRID_W = 13
GRID_H = 9
GRID_C = 40
GRID_FLAT = GRID_W * GRID_H * GRID_C  # 4680
GLOBAL_DIM = 442
INPUT_DIM = GRID_FLAT + GLOBAL_DIM   # 5122

CONV_CH = 32
REDUCE_CH = 4
MLP_IN = (GRID_W * GRID_H * REDUCE_CH) + (CONV_CH * 2) + GLOBAL_DIM  # 974
FC1_DIM = 192
FC2_DIM = 96

MAGIC = 0x4D465356  # "MFSV"
MAX_SAMPLES = 120_000

DATA_GLOB = "bin/data/Config/selfplay_spatial*.bin"
CHAMPION = "bin/data/Config/ai_value_net.bin"
WEIGHTS_OUT = "bin/data/Config/ai_value_net_candidate.bin"


class SpatialValueNet(nn.Module):
    def __init__(self, drop=0.20):
        super().__init__()
        self.conv1 = nn.Conv2d(GRID_C, CONV_CH, kernel_size=3, padding=1, bias=True)
        self.ln1 = nn.LayerNorm(CONV_CH)
        self.conv2 = nn.Conv2d(CONV_CH, CONV_CH, kernel_size=3, padding=1, bias=True)
        self.ln2 = nn.LayerNorm(CONV_CH)
        self.conv_red = nn.Conv2d(CONV_CH, REDUCE_CH, kernel_size=1, padding=0, bias=True)

        self.fc1 = nn.Linear(MLP_IN, FC1_DIM)
        self.ln_fc1 = nn.LayerNorm(FC1_DIM)
        self.fc2 = nn.Linear(FC1_DIM, FC2_DIM)
        self.ln_fc2 = nn.LayerNorm(FC2_DIM)
        self.out = nn.Linear(FC2_DIM, 1)
        self.drop = nn.Dropout(drop)

    def forward(self, x):
        b = x.size(0)
        x_grid = x[:, :GRID_FLAT].view(b, GRID_H, GRID_W, GRID_C).permute(0, 3, 1, 2)
        x_glob = x[:, GRID_FLAT:]

        h1 = self.conv1(x_grid).permute(0, 2, 3, 1)
        h1 = torch.relu(self.ln1(h1))
        h1_nchw = h1.permute(0, 3, 1, 2)

        h2 = self.conv2(h1_nchw).permute(0, 2, 3, 1)
        h2 = torch.relu(self.ln2(h2) + h1)
        h2_nchw = h2.permute(0, 3, 1, 2)

        red = torch.relu(self.conv_red(h2_nchw)).permute(0, 2, 3, 1).contiguous().view(b, -1)
        mean_pool = h2.mean(dim=(1, 2))
        max_pool = h2.amax(dim=(1, 2))

        feat = torch.cat([red, mean_pool, max_pool, x_glob], dim=1)
        f1 = self.drop(torch.relu(self.ln_fc1(self.fc1(feat))))
        f2 = self.drop(torch.relu(self.ln_fc2(self.fc2(f1))))
        return torch.tanh(self.out(f2))


def export_weights(model, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(struct.pack("<Iiiiiiiii", MAGIC, GRID_W, GRID_H, GRID_C, GLOBAL_DIM, CONV_CH, REDUCE_CH, FC1_DIM, FC2_DIM))
        layers = [
            ("conv1.weight", True), ("conv1.bias", False), ("ln1.weight", False), ("ln1.bias", False),
            ("conv2.weight", True), ("conv2.bias", False), ("ln2.weight", False), ("ln2.bias", False),
            ("conv_red.weight", True), ("conv_red.bias", False),
            ("fc1.weight", False), ("fc1.bias", False), ("ln_fc1.weight", False), ("ln_fc1.bias", False),
            ("fc2.weight", False), ("fc2.bias", False), ("ln_fc2.weight", False), ("ln_fc2.bias", False),
            ("out.weight", False), ("out.bias", False),
        ]
        sd = model.state_dict()
        for name, is_conv in layers:
            t = sd[name].detach().cpu()
            if is_conv:
                t = t.permute(0, 2, 3, 1).contiguous()
            f.write(np.ascontiguousarray(t.numpy(), dtype=np.float32).tobytes())


def try_load_champion(model, champ_path):
    if not os.path.exists(champ_path):
        return False
    try:
        with open(champ_path, "rb") as cf:
            hdr = cf.read(36)
            if len(hdr) < 36:
                return False
            magic, gw, gh, gc, gdim, cch, rch, f1, f2 = struct.unpack("<Iiiiiiiii", hdr)
            if (magic, gw, gh, gc, gdim, cch, rch, f1, f2) != (
                MAGIC, GRID_W, GRID_H, GRID_C, GLOBAL_DIM, CONV_CH, REDUCE_CH, FC1_DIM, FC2_DIM
            ):
                return False
            layers = [
                ("conv1.weight", True), ("conv1.bias", False), ("ln1.weight", False), ("ln1.bias", False),
                ("conv2.weight", True), ("conv2.bias", False), ("ln2.weight", False), ("ln2.bias", False),
                ("conv_red.weight", True), ("conv_red.bias", False),
                ("fc1.weight", False), ("fc1.bias", False), ("ln_fc1.weight", False), ("ln_fc1.bias", False),
                ("fc2.weight", False), ("fc2.bias", False), ("ln_fc2.weight", False), ("ln_fc2.bias", False),
                ("out.weight", False), ("out.bias", False),
            ]
            sd = model.state_dict()
            for name, is_conv in layers:
                t = sd[name]
                arr = np.frombuffer(cf.read(t.numel() * 4), dtype=np.float32).copy()
                if len(arr) != t.numel():
                    return False
                if is_conv:
                    c_out, c_in, kh, kw = t.shape
                    sd[name] = torch.from_numpy(arr.reshape((c_out, kh, kw, c_in))).permute(0, 3, 1, 2).contiguous()
                else:
                    sd[name] = torch.from_numpy(arr.reshape(t.shape))
            model.load_state_dict(sd)
            return True
    except Exception as e:
        print("Could not warm-start from champion, training fresh:", e)
        return False


def main():
    torch.set_num_threads(8)

    files = sorted(glob.glob(DATA_GLOB))
    if not files:
        print("No training files matched:", DATA_GLOB)
        return

    chunks = []
    stride = INPUT_DIM + 1
    sample_bytes = stride * 4
    for p in files:
        fsize = os.path.getsize(p)
        if fsize == 0 or (fsize % sample_bytes) != 0:
            continue
        arr = np.fromfile(p, dtype=np.float32)
        n = len(arr) // stride
        if n > 0:
            chunks.append(arr[: n * stride].reshape(n, stride))

    if not chunks:
        print("No valid samples found matching stride", stride)
        return

    train_parts, val_parts = [], []
    for c in chunks:
        nv = max(1, int(len(c) * 0.1))
        if len(c) > nv:
            train_parts.append(c[:-nv])
            val_parts.append(c[-nv:])
        else:
            train_parts.append(c)

    train_data = np.concatenate(train_parts, axis=0)
    val_data = np.concatenate(val_parts, axis=0) if val_parts else train_data[-max(1, len(train_data) // 10):]
    if len(train_data) > MAX_SAMPLES:
        train_data = train_data[-MAX_SAMPLES:]
    if len(val_data) > MAX_SAMPLES // 9:
        val_data = val_data[-(MAX_SAMPLES // 9):]

    n_total = len(train_data) + len(val_data)
    if n_total < 10:
        print(f"Too few samples ({n_total}) to train meaningfully.")
        return

    rng = np.random.default_rng()
    rng.shuffle(train_data)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    X_train = torch.from_numpy(train_data[:, :INPUT_DIM]).to(device)
    y_train = torch.from_numpy(train_data[:, INPUT_DIM : INPUT_DIM + 1]).to(device)
    X_val = torch.from_numpy(val_data[:, :INPUT_DIM]).to(device)
    y_val = torch.from_numpy(val_data[:, INPUT_DIM : INPUT_DIM + 1]).to(device)

    labels_all = train_data[:, INPUT_DIM]
    print(
        f"Loaded {n_total} spatial samples ({len(files)} file(s)) on {device} | Train: {len(X_train)} | Val: {len(X_val)} "
        f"| Labels: >0={(labels_all > 0).mean():.1%}, <0={(labels_all < 0).mean():.1%}"
    )

    model = SpatialValueNet(drop=0.20).to(device)
    resumed = try_load_champion(model, CHAMPION)
    if resumed:
        print("Warm-started spatial weights from existing champion:", CHAMPION)

    y_flat = y_train.squeeze(1)
    n_pos = max(1, int((y_flat > 0).sum().item()))
    n_neg = max(1, int((y_flat < 0).sum().item()))
    w_train = torch.ones_like(y_train)
    w_train[y_train > 0] = min(4.0, max(0.25, (n_pos + n_neg) / (2.0 * n_pos)))
    w_train[y_train < 0] = min(4.0, max(0.25, (n_pos + n_neg) / (2.0 * n_neg)))

    lr = 1.0e-4 if resumed else 4.0e-4
    epochs = 5 if resumed else 16
    batch_size = 1024 if device.type == "cpu" else 2048
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=5e-3)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs)
    loss_fn = nn.MSELoss()

    n_train = len(X_train)
    best_val = float("inf")
    best_state = None
    for epoch in range(epochs):
        model.train()
        perm = torch.randperm(n_train, device=device)
        train_loss = 0.0
        for start in range(0, n_train, batch_size):
            idx = perm[start : start + batch_size]
            bx, by, bw = X_train[idx], y_train[idx], w_train[idx]
            opt.zero_grad()
            pred = model(bx)
            loss = (bw * (pred - by) ** 2).mean()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
            train_loss += loss.item() * len(bx)
        sched.step()

        model.eval()
        with torch.no_grad():
            val_preds = []
            for start in range(0, len(X_val), batch_size):
                val_preds.append(model(X_val[start : start + batch_size]))
            val_pred = torch.cat(val_preds, dim=0)
            val_loss = loss_fn(val_pred, y_val).item()
            decisive = (y_val != -0.35) & (y_val != 0)
            sign_acc = ((val_pred[decisive] > 0) == (y_val[decisive] > 0)).float().mean().item() if decisive.any() else 0.0
        if val_loss < best_val:
            best_val = val_loss
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}
        print(f"Epoch {epoch+1:02d}/{epochs} | Train MSE: {train_loss / n_train:.4f} | Val MSE: {val_loss:.4f} | Val Sign Acc: {sign_acc:.1%}")

    if best_state is not None:
        model.load_state_dict(best_state)
        print(f"Restored best validation checkpoint (Val MSE: {best_val:.4f})")

    export_weights(model, WEIGHTS_OUT)
    print("Exported candidate spatial weights to", WEIGHTS_OUT)


if __name__ == "__main__":
    main()
