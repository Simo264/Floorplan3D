import os
import sys
import argparse
import pandas as pd
import matplotlib.pyplot as plt

def load_segments(filename):
    if not os.path.exists(filename):
        print(f"Error: file not found: '{filename}'")
        return []
    try:
        df = pd.read_csv(filename, header=None, dtype=float)
        segments = [((row[0], row[1]), (row[2], row[3])) for row in df.itertuples(index=False)]
        return segments
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        return []

def main():
    parser = argparse.ArgumentParser(description="Plot wall/door/window segments from CSV files.")
    parser.add_argument("output", nargs="?", default="out/segments.jpg", help="Output image path (default: out/segments.jpg)")
    args = parser.parse_args()

    wall_segments = load_segments("out/walls.csv")
    door_segments = load_segments("out/doors.csv")
    window_segments = load_segments("out/windows.csv")

    fig, ax = plt.subplots(figsize=(10, 8))

    if wall_segments:
        for i, (p1, p2) in enumerate(wall_segments):
            label = "Walls" if i == 0 else ""
            ax.plot([p1[0], p2[0]], [p1[1], p2[1]], color="black", linewidth=1.50, label=label)

    if door_segments:
        for i, (dp1, dp2) in enumerate(door_segments):
            label = "Door" if i == 0 else ""
            ax.plot([dp1[0], dp2[0]], [dp1[1], dp2[1]], color="red", linewidth=1.25, label=label)

    if window_segments:
        for i, (wp1, wp2) in enumerate(window_segments):
            label = "Windows" if i == 0 else ""
            ax.plot([wp1[0], wp2[0]], [wp1[1], wp2[1]], color="blue", linewidth=1.25, label=label)

    ax.set_aspect("equal")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.grid(True, linestyle=":", alpha=0.5)
    ax.legend(loc="upper left")

    # Assicurati che la directory di output esista
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)

    plt.savefig(args.output, dpi=300, bbox_inches='tight')
    print(f"Image saved: {args.output}")

if __name__ == "__main__":
    main()