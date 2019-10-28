#!/usr/bin/env python3
from matplotlib import pyplot as plt, lines
import shelve
import json
import subprocess as subp
import sys
from collections import defaultdict
from run_benchmarks import get_commits, run
import numpy as np
import threading

thread = None
current_index = 0

commits, comments = get_commits()

def get_benchmarks(results):
    benchmarks = defaultdict(lambda: [])
    for hash in commits:
        if hash in results and results[hash] is not None:
            benchs = results[hash]
            for b in benchs["benchmarks"]:
                name = b["name"]
                time = min(b["cpu_time"], b["real_time"])
                benchmarks[name].append((commits.index(hash), time))
    return benchmarks

with shelve.open("benchmark_results") as results:
    benchmarks = get_benchmarks(results)

fig, ax = plt.subplots(4, 1, figsize=(10, 10), sharex=True)
plt.subplots_adjust(hspace=0, top=0.98, bottom=0.05, right=0.96)

plt.sca(ax[0])
for name, xy in benchmarks.items():
    if "uniform" in name: continue
    if "_1d" in name:
        x, y = np.transpose(xy)
        plt.plot(x, y, ".-", label=name)
plt.legend(fontsize="xx-small")

plt.sca(ax[1])
for name, xy in benchmarks.items():
    if "uniform" in name: continue
    if "_2d" in name:
        x, y = np.transpose(xy)
        plt.plot(x, y, ".-", label=name)
plt.legend(fontsize="xx-small")

plt.sca(ax[2])
for name, xy in benchmarks.items():
    if "uniform" in name: continue
    if "_3d" in name:
        x, y = np.transpose(xy)
        plt.plot(x, y, ".-", label=name)
plt.legend(fontsize="xx-small")

plt.sca(ax[3])
for name, xy in benchmarks.items():
    if "uniform" in name: continue
    if "_6d" in name:
        x, y = np.transpose(xy)
        plt.plot(x, y, ".-", label=name)
plt.legend(fontsize="xx-small")

plt.figtext(0.01, 0.5, "time per loop / ns [smaller is better]", rotation=90, va="center")

def format_coord(x, y):
    global current_index
    current_index = max(0, min(int(x + 0.5), len(commits) - 1))
    hash = commits[current_index]
    comment = comments[hash]
    return f"{hash} {comment}"

for axi in ax.flatten():
    axi.format_coord = format_coord

def on_key_press(event):
    global thread
    if thread and thread.is_alive(): return

    if event.key != "u": return

    hash = commits[current_index]

    def worker(fig, ax, hash):
        with shelve.open("benchmark_results") as results:
            run(results, comments, hash, True)
            benchmarks = get_benchmarks(results)

        for name in benchmarks:
            xy = benchmarks[name]
            x, y = np.transpose(xy)
            for axi in ax.flatten():
                for artist in axi.get_children():
                    if isinstance(artist, lines.Line2D) and artist.get_label() == name:
                        artist.set_xdata(x)
                        artist.set_ydata(y)

        fig.canvas.draw()

    thread = threading.Thread(target=worker, args=(fig, ax, hash))
    thread.start()

fig.canvas.mpl_connect('key_press_event', on_key_press)

plt.show()
