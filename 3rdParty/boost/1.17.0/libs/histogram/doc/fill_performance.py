#!/usr/bin/env python3
import os
import numpy as np
import glob
import re
import json
from collections import defaultdict, OrderedDict
from matplotlib.patches import Rectangle
from matplotlib.lines import Line2D
from matplotlib.text import Text
from matplotlib.font_manager import FontProperties
import matplotlib.pyplot as plt
import matplotlib as mpl
mpl.rcParams.update(mpl.rcParamsDefault)

data = defaultdict(lambda:[])
for fn in glob.glob("fill_*.json"):
    for bench in json.load(open(fn))["benchmarks"]:
        name = bench["name"]
        time = min(bench["cpu_time"], bench["real_time"])
        m = re.match("fill_([0-9])d<([^>]+)>", name)
        tags = m.group(2).split(", ")
        dim = int(m.group(1))
        label = re.search("fill_([a-z]+).json", fn).group(1)
        dist = tags[0]
        if label == "boost":
            label += "-" + {"dynamic_tag":"D", "static_tag":"S"}[tags[1]] + tags[2][0]
        data[dim].append((label, dist, time / dim))

plt.figure()
if os.path.exists("/proc/cpuinfo"):
    cpuinfo = open("/proc/cpuinfo").read()
    m = re.search("model name\s*:\s*(.+)\n", cpuinfo)
    if m:
        plt.title(m.group(1))
i = 0
for dim in sorted(data):
    v = data[dim]
    labels = OrderedDict()
    for label, dist, time in v:
        if label in labels:
            labels[label][dist] = time
        else:
            labels[label] = {dist: time}
    j = 0
    for label, d in labels.items():
        t1 = d["uniform"]
        t2 = d["normal"]
        i -= 1
        z = float(j) / len(labels)
        col = ((1.0-z) * np.array((1.0, 0.0, 0.0))
               + z * np.array((1.0, 1.0, 0.0)))
        if label == "root":
            col = "k"
        if "numpy" in label:
            col = "0.6"
        if "gsl" in label:
            col = "0.3"
        tmin = min(t1, t2)
        tmax = max(t1, t2)
        r1 = Rectangle((0, i), tmax, 1, facecolor=col)
        r2 = Rectangle((tmin, i), tmax-tmin, 1, facecolor="none", edgecolor="w", hatch="//////")
        plt.gca().add_artist(r1)
        plt.gca().add_artist(r2)
        tx = Text(-0.1, i+0.5, "%s" % label,
                  va="center", ha="right", clip_on=False)
        plt.gca().add_artist(tx)
        j += 1
    i -= 1
    font0 = FontProperties()
    font0.set_weight("bold")
    tx = Text(-0.1, i+0.6, "%iD" % dim,
              fontproperties=font0, va="center", ha="right", clip_on=False)
    plt.gca().add_artist(tx)
plt.ylim(0, i)
plt.xlim(0, 20)

plt.tick_params("y", left=False, labelleft=False)
plt.xlabel("fill time per random input value in nanoseconds (smaller is better)")

plt.savefig("fill_performance.svg")
plt.show()
