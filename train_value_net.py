import glob
import os
import struct
import numpy as np
import torch
import torch.nn as nn

INPUT_DIM = 398
H1 = 256
H2 = 128
H3 = 64
MAGIC = 0x4D46564E  # "MFVN"
MAX_SAMPLES = 250_000

DATA_GLOB = "bin/data/Config/selfplay_data*.bin"
WEIGHTS_OUT = "bin/data/Config/ai_value_net_candidate.bin"


class ResidualBlock(nn.Module):
    def __init__(self, dim, drop=0.25):
        super().__init__()
        self.fc1 = nn.Linear(dim, dim)
        self.ln1 = nn.LayerNorm(dim)
        self.fc2 = nn.Linear(dim, dim)
        self.ln2 = nn.LayerNorm(dim)
        self.drop = nn.Dropout(drop)

    def forward(self, x):
        residual = x
        out = self.drop(torch.relu(self.ln1(self.fc1(x))))
        out = self.ln2(self.fc2(out))
        return torch.relu(out + residual)


class ValueNet(nn.Module):
    def __init__(self, drop=0.25):
        super().__init__()
        self.in_fc = nn.Linear(INPUT_DIM, H1)
        self.in_ln = nn.LayerNorm(H1)
        self.drop = nn.Dropout(drop)

        self.res1 = ResidualBlock(H1, drop=drop)

        self.down1 = nn.Linear(H1, H2)
        self.ln2 = nn.LayerNorm(H2)

        self.down2 = nn.Linear(H2, H3)
        self.ln3 = nn.LayerNorm(H3)

        self.out = nn.Linear(H3, 1)

    def forward(self, x):
        x = self.drop(torch.relu(self.in_ln(self.in_fc(x))))
        x = self.res1(x)
        x = self.drop(torch.relu(self.ln2(self.down1(x))))
        x = self.drop(torch.relu(self.ln3(self.down2(x))))
        return torch.tanh(self.out(x))


def main():
    torch.set_num_threads(8)  # Matches physical Zen 5 cores on 9800X3D for maximum throughput

    files = sorted(glob.glob(DATA_GLOB))
    if not files:
        print("No training files matched:", DATA_GLOB)
        return

    chunks = []
    stride = INPUT_DIM + 1
    for p in files:
        arr = np.fromfile(p, dtype=np.float32)
        n = len(arr) // stride
        if n > 0:
            chunks.append(arr[: n * stride].reshape(n, stride))

    if not chunks:
        print("No valid samples found matching stride", stride)
        return

    # Take the last 10% of each file contiguously to prevent same-game leak into validation
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

    data = np.concatenate([train_data, val_data], axis=0)
    n_total = len(data)
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

    labels_all = data[:, INPUT_DIM]
    print(
        f"Loaded {n_total} samples ({len(files)} file(s)) on {device} | Train: {len(X_train)} | Val: {len(X_val)} "
        f"| Labels: >0={(labels_all > 0).mean():.1%}, <0={(labels_all < 0).mean():.1%}, ==0={(labels_all == 0).mean():.1%}"
    )

    model = ValueNet(drop=0.25).to(device)
    resumed = False
    champ_path = "bin/data/Config/ai_value_net.bin"
    if os.path.exists(champ_path):
        try:
            with open(champ_path, "rb") as cf:
                magic, in_dim, h1, h2, h3 = struct.unpack("<Iiiii", cf.read(20))
                if magic == MAGIC and h1 == H1 and h2 == H2 and h3 == H3:
                    sd = model.state_dict()
                    for k in sd:
                        if k == "in_fc.weight" and in_dim != INPUT_DIM:
                            # Transfer learning: load matching input features, zero-pad the new ones
                            old_arr = np.frombuffer(cf.read(h1 * in_dim * 4), dtype=np.float32).copy()
                            old_w = torch.from_numpy(old_arr.reshape((h1, in_dim)))
                            new_w = torch.zeros((h1, INPUT_DIM), dtype=torch.float32)
                            copy_dim = min(in_dim, INPUT_DIM)
                            new_w[:, :copy_dim] = old_w[:, :copy_dim]
                            sd[k] = new_w
                        else:
                            needed = sd[k].numel()
                            arr = np.frombuffer(cf.read(needed * 4), dtype=np.float32).copy()
                            if len(arr) == needed:
                                sd[k] = torch.from_numpy(arr.reshape(sd[k].shape))
                    model.load_state_dict(sd)
                    resumed = True
                    print(f"Warm-started weights from existing champion (Expanded {in_dim} -> {INPUT_DIM} inputs):", champ_path)
        except Exception as e:
            print("Could not warm-start from champion, training fresh:", e)

    y_flat = y_train.squeeze(1)
    n_pos = max(1, int((y_flat > 0).sum().item()))
    n_neg = max(1, int((y_flat < 0).sum().item()))
    w_train = torch.ones_like(y_train)
    w_train[y_train > 0] = min(4.0, max(0.25, (n_pos + n_neg) / (2.0 * n_pos)))
    w_train[y_train < 0] = min(4.0, max(0.25, (n_pos + n_neg) / (2.0 * n_neg)))

    lr = 4.0e-5 if resumed else 3.0e-4
    epochs = 4 if resumed else 8
    batch_size = 2048
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-2)
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
            bx = X_train[idx]
            by = y_train[idx]
            bw = w_train[idx]
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
            val_loss = loss_fn(model(X_val), y_val).item()
        if val_loss < best_val:
            best_val = val_loss
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}
        print(f"Epoch {epoch+1:02d}/{epochs} | Train MSE: {train_loss / n_train:.4f} | Val MSE: {val_loss:.4f}")

    if best_state is not None:
        model.load_state_dict(best_state)
        print(f"Restored best validation checkpoint (Val MSE: {best_val:.4f})")

    os.makedirs(os.path.dirname(WEIGHTS_OUT), exist_ok=True)
    with open(WEIGHTS_OUT, "wb") as f:
        f.write(struct.pack("<Iiiii", MAGIC, INPUT_DIM, H1, H2, H3))
        for name, param in model.named_parameters():
            arr = np.ascontiguousarray(param.detach().cpu().numpy(), dtype=np.float32)
            f.write(arr.tobytes())

    print("Exported candidate weights to", WEIGHTS_OUT)


if __name__ == "__main__":
    main()