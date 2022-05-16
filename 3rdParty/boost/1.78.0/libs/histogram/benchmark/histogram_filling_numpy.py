from __future__ import print_function
import numpy as np
# pip install fast-histogram
from fast_histogram import histogram1d
import timeit

x = np.random.rand(1 << 20)
nrepeat = 10

print(timeit.timeit("np.histogram(x, bins=100, range=(0, 1))",
                    "from __main__ import x, np", number=nrepeat) / (nrepeat * len(x)) / 1e-9)
                    
print(timeit.timeit("histogram1d(x, bins=100, range=(0, 1))",
                    "from __main__ import x, histogram1d", number=nrepeat) / (nrepeat * len(x)) / 1e-9)