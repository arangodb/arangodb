#!/usr/bin/python

# Copyright (c) 2014 Kyle Lutz <kyle.r.lutz@gmail.com>
# Distributed under the Boost Software License, Version 1.0
# See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt
#
# See http://boostorg.github.com/compute for more information.

import os
import sys
import pylab

from perf import run_benchmark

fignum = 0

def plot_to_file(report, filename):
    global fignum
    fignum += 1
    pylab.figure(fignum)

    run_to_label = {
        "stl" : "C++ STL",
        "thrust" : "Thrust",
        "compute" : "Boost.Compute",
        "bolt" : "Bolt"
    }

    for run in sorted(report.samples.keys()):
        x = []
        y = []

        for sample in report.samples[run]:
            x.append(sample[0])
            y.append(sample[1])

        pylab.loglog(x, y, marker='o', label=run_to_label[run])

    pylab.xlabel("Size")
    pylab.ylabel("Time (ms)")
    pylab.legend(loc='upper left')
    pylab.savefig(filename)

if __name__ == '__main__':
    sizes = [pow(2, x) for x in range(10, 26)]
    algorithms = [
        "accumulate",
        "count",
        "inner_product",
        "merge",
        "partial_sum",
        "partition",
        "reverse",
        "rotate",
        "saxpy",
        "sort",
        "unique",
    ]

    try:
        os.mkdir("perf_plots")
    except OSError:
        pass

    for algorithm in algorithms:
        print("running '%s'" % (algorithm))
        report = run_benchmark(algorithm, sizes, ["stl", "thrust", "bolt"])
        plot_to_file(report, "perf_plots/%s_time_plot.png" % algorithm)

