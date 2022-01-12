#!/usr/bin/env python3

#            Copyright Hans Dembinski 2018 - 2019.
#   Distributed under the Boost Software License, Version 1.0.
#      (See accompanying file LICENSE_1_0.txt or copy at
#            https://www.boost.org/LICENSE_1_0.txt)

import os
import numpy as np
import glob
import re
import json
import sys
from collections import defaultdict, OrderedDict
from matplotlib.patches import Rectangle
from matplotlib.lines import Line2D
from matplotlib.text import Text
from matplotlib.font_manager import FontProperties
import matplotlib.pyplot as plt
import matplotlib as mpl

mpl.rcParams.update(mpl.rcParamsDefault)

cpu_frequency = 0

data = defaultdict(lambda: [])
hostname = None
for fn in sys.argv[1:]:
    d = json.load(open(fn))
    cpu_frequency = d["context"]["mhz_per_cpu"]
    # make sure we don't compare benchmarks from different computers
    if hostname is None:
        hostname = d["context"]["host_name"]
    else:
        assert hostname == d["context"]["host_name"]
    for bench in d["benchmarks"]:
        name = bench["name"]
        time = min(bench["cpu_time"], bench["real_time"])
        m = re.match("fill_(n_)?([0-9])d<([^>]+)>", name)
        if m.group(1):
            time /= 1 << 15
        tags = m.group(3).split(", ")
        dim = int(m.group(2))
        label = re.search(
            "fill_([a-z]+)", os.path.splitext(os.path.split(fn)[1])[0]
        ).group(1)
        dist = tags[0]
        if len(tags) > 1 and tags[1] in ("dynamic_tag", "static_tag"):
            if len(tags) == 3 and "DStore" in tags[2]:
                continue
            label += "-" + {"dynamic_tag": "dyn", "static_tag": "sta"}[tags[1]]
            label += "-fill" if m.group(1) else "-call"
        data[dim].append((label, dist, time / dim))

time_per_cycle_in_ns = 1.0 / (cpu_frequency * 1e6) / 1e-9

plt.figure(figsize=(7, 6))
i = 0
for dim in sorted(data):
    v = data[dim]
    labels = OrderedDict()
    for label, dist, time in v:
        if label in labels:
            labels[label][dist] = time / time_per_cycle_in_ns
        else:
            labels[label] = {dist: time / time_per_cycle_in_ns}
    j = 0
    for label, d in labels.items():
        t1 = d["uniform"]
        t2 = d["normal"]
        i -= 1
        z = float(j) / len(labels)
        col = (1.0 - z) * np.array((1.0, 0.0, 0.0)) + z * np.array((1.0, 1.0, 0.0))
        if label == "root":
            col = "k"
            label = "ROOT 6"
        if "numpy" in label:
            col = "0.6"
        if "gsl" in label:
            col = "0.3"
            label = "GSL"
        tmin = min(t1, t2)
        tmax = max(t1, t2)
        r1 = Rectangle((0, i), tmax, 1, facecolor=col)
        r2 = Rectangle(
            (tmin, i), tmax - tmin, 1, facecolor="none", edgecolor="w", hatch="//////"
        )
        plt.gca().add_artist(r1)
        plt.gca().add_artist(r2)
        font = FontProperties(size=9)
        tx = Text(
            -0.5,
            i + 0.5,
            "%s" % label,
            fontproperties=font,
            va="center",
            ha="right",
            clip_on=False,
        )
        plt.gca().add_artist(tx)
        j += 1
    i -= 1
    font = FontProperties()
    font.set_weight("bold")
    tx = Text(
        -0.5,
        i + 0.6,
        "%iD" % dim,
        fontproperties=font,
        va="center",
        ha="right",
        clip_on=False,
    )
    plt.gca().add_artist(tx)
plt.ylim(0, i)
plt.xlim(0, 80)
from matplotlib.ticker import MultipleLocator

plt.gca().xaxis.set_major_locator(MultipleLocator(5))

plt.tick_params("y", left=False, labelleft=False)
plt.xlabel("average CPU cycles per random input value (smaller is better)")

plt.tight_layout()

plt.savefig("fill_performance.svg")
plt.show()
