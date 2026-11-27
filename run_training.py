import glob
import os
import re
import shutil
import struct
import subprocess
import time

EXE_PATH = "bin\\MageFight.exe" if os.name == "nt" else "./bin/MageFight"
NUM_WORKERS = 14              # 14 parallel threads on 9800X3D
SELFPLAY_SECONDS = 90         # 3.6x more self-play games per generation
ARENA_WORKERS = 14
ARENA_GAMES_PER_WORKER = 12   # 168 Arena games total (~1.5-2 min instead of 4+ min)
PROMOTE_THRESHOLD = 53.5      # Requires a clear win over the champion (AlphaZero standard)
HISTORY_KEEP = 1              # Keep buffer fresh on the current champion
INPUT_DIM = 6563              # 13*9*48 (5616) + 947 global features
CHAMPION = "bin/data/Config/ai_value_net.bin"
BOOTSTRAP = "bin/data/Config/spatial_bootstrap.bin"
CANDIDATE = "bin/data/Config/ai_value_net_candidate.bin"
SPATIAL_MAGIC = 0x4D465356


def is_spatial_champion(path: str) -> bool:
    if not os.path.exists(path):
        return False
    try:
        with open(path, "rb") as f:
            hdr = f.read(4)
        return len(hdr) == 4 and struct.unpack("<I", hdr)[0] == SPATIAL_MAGIC
    except OSError:
        return False


def get_annealed_heuristic_weight(gen: int) -> float:
    if gen <= 3:
        return 1.0
    if gen <= 10:
        return max(0.20, 1.0 - (gen - 3) * 0.114)
    return max(0.0, 0.20 - (gen - 10) * 0.02)


def cleanup_procs(procs):
    for p in procs:
        if p.poll() is None:
            p.terminate()
    for p in procs:
        try:
            p.wait(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill()


def run_generation(gen: int):
    h_weight = get_annealed_heuristic_weight(gen)
    print(f"\n================ [GENERATION {gen}] ================")
    print(f"Heuristic Weight: {h_weight:.2f} (Neural Net Authority: {(1.0 - h_weight)*100:.1f}%)")

    os.makedirs("bin/data/Config", exist_ok=True)
    worker_files = sorted(glob.glob("bin/data/Config/selfplay_spatial_[0-9]*.bin"))
    if worker_files:
        hist_name = f"bin/data/Config/selfplay_spatial_hist_{int(time.time())}.bin"
        with open(hist_name, "wb") as hf:
            for wf in worker_files:
                with open(wf, "rb") as f:
                    shutil.copyfileobj(f, hf)
        for wf in worker_files:
            try:
                os.remove(wf)
            except OSError:
                pass
    hist_files = sorted(glob.glob("bin/data/Config/selfplay_spatial_hist_*.bin"))
    for stale in hist_files[:-HISTORY_KEEP]:
        try:
            os.remove(stale)
        except OSError:
            pass

    print(f"[{time.strftime('%H:%M:%S')}] Spawning {NUM_WORKERS} headless self-play workers for {SELFPLAY_SECONDS}s...")
    procs = []
    try:
        for wid in range(1, NUM_WORKERS + 1):
            env = os.environ.copy()
            env["MAGEFIGHT_HEADLESS"] = "1"
            env["MAGEFIGHT_SELFPLAY"] = "1"
            env["MAGEFIGHT_WORKER_ID"] = str(wid)
            if wid > NUM_WORKERS - 2 and gen > 3:
                env["MAGEFIGHT_HEURISTIC_WEIGHT"] = "1.0"
            else:
                env["MAGEFIGHT_HEURISTIC_WEIGHT"] = str(h_weight)
            procs.append(
                subprocess.Popen(
                    [EXE_PATH],
                    env=env,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
            )
        time.sleep(SELFPLAY_SECONDS)
    finally:
        print(f"[{time.strftime('%H:%M:%S')}] Stopping self-play workers and harvesting game logs...")
        for idx, p in enumerate(procs, 1):
            rc = p.poll()
            if rc is not None and rc != 0:
                print(f"Warning: Worker {idx} exited early with code {rc}")
        cleanup_procs(procs)

    data_files = glob.glob("bin/data/Config/selfplay_spatial*.bin")
    total_bytes = sum(os.path.getsize(f) for f in data_files)
    stride_bytes = INPUT_DIM * 4 + 4
    total_samples = total_bytes // stride_bytes
    print(f"[{time.strftime('%H:%M:%S')}] Harvested ~{total_samples} spatial samples across {len(data_files)} buffer files.")

    if total_samples < 100:
        print("Warning: Too few samples harvested in this window. Skipping training for this cycle.")
        return

    print(f"[{time.strftime('%H:%M:%S')}] Training PyTorch spatial neural network...")
    py_exe = "python" if os.name == "nt" else "python3"
    train_res = subprocess.run([py_exe, "train_value_net.py"])
    if train_res.returncode != 0 or not os.path.exists(CANDIDATE):
        print("Training failed or produced no candidate weights.")
        return

    if not os.path.exists(CHAMPION):
        shutil.move(CANDIDATE, CHAMPION)
        shutil.copy2(CHAMPION, f"bin/data/Config/champion_gen_{gen}.bin")
        print(f"[{time.strftime('%H:%M:%S')}] Initial champion created and saved to {CHAMPION}!")
        return

    total_arena_target = ARENA_WORKERS * ARENA_GAMES_PER_WORKER
    print(f"[{time.strftime('%H:%M:%S')}] Running {total_arena_target}-game Parallel Arena Gauntlet ({ARENA_WORKERS} workers x {ARENA_GAMES_PER_WORKER} games)...")
    arena_procs = []
    total_p0 = 0
    total_p1 = 0
    total_draws = 0
    try:
        for wid in range(1, ARENA_WORKERS + 1):
            arena_env = os.environ.copy()
            arena_env["MAGEFIGHT_HEADLESS"] = "1"
            arena_env["MAGEFIGHT_ARENA"] = "1"
            if wid > ARENA_WORKERS // 2:
                arena_env["MAGEFIGHT_ARENA_SWAP"] = "1"
            arena_env["MAGEFIGHT_ARENA_GAMES"] = str(ARENA_GAMES_PER_WORKER)
            arena_env["MAGEFIGHT_WORKER_ID"] = str(gen * 100 + wid)
            arena_env["MAGEFIGHT_HEURISTIC_WEIGHT"] = str(h_weight)
            arena_procs.append(
                subprocess.Popen(
                    [EXE_PATH],
                    env=arena_env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL,
                    text=True,
                )
            )

        for p in arena_procs:
            out, _ = p.communicate()
            matches = re.findall(r"\[Arena\] Game \d+ \| Baseline\(P0\): (\d+) \| Candidate\(P1\): (\d+) \| Draws: (\d+)", out)
            if matches:
                p0, p1, dr = map(int, matches[-1])
                total_p0 += p0
                total_p1 += p1
                total_draws += dr
    finally:
        cleanup_procs(arena_procs)

    total_played = total_p0 + total_p1 + total_draws
    cand_score = (100.0 * (total_p1 + 0.5 * total_draws) / total_played) if total_played > 0 else 0.0
    decisive_played = total_p0 + total_p1
    decisive_rate = (100.0 * total_p1 / decisive_played) if decisive_played > 0 else 0.0

    print(
        f"[{time.strftime('%H:%M:%S')}] Arena Summary ({total_played} games): "
        f"Baseline(P0)={total_p0} | Candidate(P1)={total_p1} | Draws={total_draws} "
        f"| Overall Score: {cand_score:.2f}% | Decisive Winrate: {decisive_rate:.2f}%"
    )

    # Require both overall score >= 53.5% AND decisive winrate >= 54.0% to beat RNG noise
    should_promote = (cand_score >= PROMOTE_THRESHOLD) and (decisive_rate >= 54.0) and (total_p1 > total_p0)

    if total_played > 0 and should_promote:
        shutil.move(CANDIDATE, CHAMPION)
        shutil.copy2(CHAMPION, f"bin/data/Config/champion_gen_{gen}.bin")
        if os.path.exists(BOOTSTRAP):
            try:
                os.remove(BOOTSTRAP)
            except OSError:
                pass
        print(f"[{time.strftime('%H:%M:%S')}] 🎉 SUCCESS: Candidate WON the gauntlet and is now the NEW CHAMPION!")
    else:
        if not is_spatial_champion(CHAMPION):
            shutil.move(CANDIDATE, BOOTSTRAP)
            print(f"[{time.strftime('%H:%M:%S')}] Saved spatial progress to {BOOTSTRAP} for next generation warm-start.")
        else:
            try:
                os.remove(CANDIDATE)
            except OSError:
                pass
            print(f"[{time.strftime('%H:%M:%S')}] ❌ DEFEAT: Candidate failed to beat the Champion. Discarded.")


def get_latest_generation_number():
    checkpoints = glob.glob("bin/data/Config/champion_gen_*.bin")
    max_gen = 0
    for path in checkpoints:
        m = re.search(r"champion_gen_(\d+)\.bin$", path)
        if m:
            max_gen = max(max_gen, int(m.group(1)))
    if max_gen > 0:
        return max_gen + 1
    if os.path.exists(CHAMPION):
        return 2
    return 1


if __name__ == "__main__":
    gen = get_latest_generation_number()
    print(f"Starting automated self-play training loop at Generation {gen}...")
    try:
        while True:
            run_generation(gen)
            gen += 1
    except KeyboardInterrupt:
        print("\n\nTraining loop paused by user (Ctrl+C). Your champion is safely saved in bin/data/Config/ai_value_net.bin")