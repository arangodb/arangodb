"""
 Copyright 2011-2014 Mario Mulansky
 Copyright 2011-2014 Karsten Ahnert

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
"""

import numpy as np
from matplotlib import pyplot as plt

plt.rc("font", size=16)


def get_runtime_from_file(filename):
    gcc_perf_file = open(filename, 'r')
    for line in gcc_perf_file:
        if "Minimal Runtime:" in line:
            return float(line.split(":")[-1])


t_gcc = [get_runtime_from_file("perf_workbook/odeint_rk4_array_gcc.perf"),
         get_runtime_from_file("perf_ariel/odeint_rk4_array_gcc.perf"),
         get_runtime_from_file("perf_lyra/odeint_rk4_array_gcc.perf")]

t_intel = [get_runtime_from_file("perf_workbook/odeint_rk4_array_intel.perf"),
           get_runtime_from_file("perf_ariel/odeint_rk4_array_intel.perf"),
           get_runtime_from_file("perf_lyra/odeint_rk4_array_intel.perf")]

t_gfort = [get_runtime_from_file("perf_workbook/rk4_gfort.perf"),
           get_runtime_from_file("perf_ariel/rk4_gfort.perf"),
           get_runtime_from_file("perf_lyra/rk4_gfort.perf")]

t_c_intel = [get_runtime_from_file("perf_workbook/rk4_c_intel.perf"),
             get_runtime_from_file("perf_ariel/rk4_c_intel.perf"),
             get_runtime_from_file("perf_lyra/rk4_c_intel.perf")]

print t_c_intel


ind = np.arange(3)  # the x locations for the groups
width = 0.15         # the width of the bars

fig = plt.figure()
ax = fig.add_subplot(111)
rects1 = ax.bar(ind, t_gcc, width, color='b', label="odeint gcc")
rects2 = ax.bar(ind+width, t_intel, width, color='g', label="odeint intel")
rects3 = ax.bar(ind+2*width, t_c_intel, width, color='y', label="C intel")
rects4 = ax.bar(ind+3*width, t_gfort, width, color='c', label="gfort")

ax.axis([-width, 2.0+5*width, 0.0, 0.85])
ax.set_ylabel('Runtime (s)')
ax.set_title('Performance for integrating the Lorenz system')
ax.set_xticks(ind + 1.5*width)
ax.set_xticklabels(('Core i5-3210M\n3.1 GHz',
                    'Xeon E5-2690\n3.8 GHz',
                    'Opteron 8431\n 2.4 GHz'))
ax.legend(loc='upper left', prop={'size': 16})

plt.savefig("perf.pdf")
plt.savefig("perf.png", dpi=50)

plt.show()
