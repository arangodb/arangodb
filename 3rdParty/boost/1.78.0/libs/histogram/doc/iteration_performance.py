#!/usr/bin/env python3

#            Copyright Hans Dembinski 2018 - 2019.
#   Distributed under the Boost Software License, Version 1.0.
#      (See accompanying file LICENSE_1_0.txt or copy at
#            https://www.boost.org/LICENSE_1_0.txt)

import numpy as np
import json
from collections import defaultdict
import os
import re
import sys
import matplotlib.pyplot as plt
import matplotlib as mpl
mpl.rcParams.update(mpl.rcParamsDefault)

bench = defaultdict(lambda:[])
data = json.load(open(sys.argv[1]))

for benchmark in data["benchmarks"]:
    # Naive/(tuple, 3, inner)/4    3.44 ns
    m = re.match("(\S+)/\((\S+), (\d), (\S+)\)/(\d+)", benchmark["name"])
    name = m.group(1)
    hist = m.group(2)
    dim = int(m.group(3))
    cov = m.group(4)
    nbins = int(m.group(5))
    time = benchmark["cpu_time"]
    bench[(name, hist, dim, cov)].append((int(nbins) ** dim, time))

fig, ax = plt.subplots(1, 3, figsize=(10, 5), sharex=True, sharey=True)
if os.path.exists("/proc/cpuinfo"):
    cpuinfo = open("/proc/cpuinfo").read()
    m = re.search("model name\s*:\s*(.+)\n", cpuinfo)
    if m:
        plt.suptitle(m.group(1))
plt.subplots_adjust(bottom=0.18, wspace=0, top=0.85, right=0.98, left=0.07)
for iaxis, axis_type in enumerate(("tuple", "vector", "vector_of_variant")):
    plt.sca(ax[iaxis])
    plt.title(axis_type.replace("_", " "), y=1.02)
    handles = []
    for (name, axis_t, dim, cov), v in bench.items():
        if axis_t != axis_type: continue
        if cov != "inner": continue
        v = np.sort(v, axis=0).T
        # if "semi_dynamic" in axis: continue
        name2, col, ls = {
            "Naive": ("nested for", "0.5", ":"),
            "Indexed": ("indexed", "r", "-")}.get(name, (name, "k", "-"))
        h = plt.plot(v[0], v[1] / v[0], color=col, ls=ls, lw=dim,
                     label=r"%s: $D=%i$" % (name2, dim))[0]
        handles.append(h)
        handles.sort(key=lambda x: x.get_label())
    plt.loglog()
plt.sca(ax[0])
plt.ylabel("CPU time in ns per bin")
plt.legend(handles=handles, fontsize="x-small", frameon=False, handlelength=4, ncol=2)
plt.figtext(0.5, 0.05, "number of bins", ha="center")
plt.show()
