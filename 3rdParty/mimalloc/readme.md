
<img align="left" width="100" height="100" src="doc/mimalloc-logo.png"/>

[<img align="right" src="https://dev.azure.com/Daan0324/mimalloc/_apis/build/status/microsoft.mimalloc?branchName=dev"/>](https://dev.azure.com/Daan0324/mimalloc/_build?definitionId=1&_a=summary)

# mimalloc

&nbsp;

mimalloc (pronounced "me-malloc")
is a general purpose allocator with excellent [performance](#performance) characteristics.
Initially developed by Daan Leijen for the run-time systems of the
[Koka](https://github.com/koka-lang/koka) and [Lean](https://github.com/leanprover/lean) languages.

It is a drop-in replacement for `malloc` and can be used in other programs
without code changes, for example, on dynamically linked ELF-based systems (Linux, BSD, etc.) you can use it as:
```
> LD_PRELOAD=/usr/bin/libmimalloc.so  myprogram
```

Notable aspects of the design include:

- __small and consistent__: the library is about 6k LOC using simple and
  consistent data structures. This makes it very suitable
  to integrate and adapt in other projects. For runtime systems it
  provides hooks for a monotonic _heartbeat_ and deferred freeing (for
  bounded worst-case times with reference counting).
- __free list sharding__: the big idea: instead of one big free list (per size class) we have
  many smaller lists per memory "page" which both reduces fragmentation
  and increases locality --
  things that are allocated close in time get allocated close in memory.
  (A memory "page" in _mimalloc_ contains blocks of one size class and is
  usually 64KiB on a 64-bit system).
- __eager page reset__: when a "page" becomes empty (with increased chance
  due to free list sharding) the memory is marked to the OS as unused ("reset" or "purged")
  reducing (real) memory pressure and fragmentation, especially in long running
  programs.
- __secure__: _mimalloc_ can be built in secure mode, adding guard pages,
  randomized allocation, encrypted free lists, etc. to protect against various
  heap vulnerabilities. The performance penalty is usually around 10% on average
  over our benchmarks.
- __first-class heaps__: efficiently create and use multiple heaps to allocate across different regions.
  A heap can be destroyed at once instead of deallocating each object separately.  
- __bounded__: it does not suffer from _blowup_ \[1\], has bounded worst-case allocation
  times (_wcat_), bounded space overhead (~0.2% meta-data, with at most 12.5% waste in allocation sizes),
  and has no internal points of contention using only atomic operations.
- __fast__: In our benchmarks (see [below](#performance)),
  _mimalloc_ always outperforms all other leading allocators (_jemalloc_, _tcmalloc_, _Hoard_, etc),
  and usually uses less memory (up to 25% more in the worst case). A nice property
  is that it does consistently well over a wide range of benchmarks.

The [documentation](https://microsoft.github.io/mimalloc) gives a full overview of the API.
You can read more on the design of _mimalloc_ in the [technical report](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action) which also has detailed benchmark results.   

Enjoy!  

### Releases

* 2019-12-22, `v1.2.2`: stable release 1.2: minor updates.
* 2019-11-22, `v1.2.0`: stable release 1.2: bug fixes, improved secure mode (free list corruption checks, double free mitigation). Improved dynamic overriding on Windows.
* 2019-10-07, `v1.1.0`: stable release 1.1.
* 2019-09-01, `v1.0.8`: pre-release 8: more robust windows dynamic overriding, initial huge page support.
* 2019-08-10, `v1.0.6`: pre-release 6: various performance improvements.

# Building

## Windows

Open `ide/vs2019/mimalloc.sln` in Visual Studio 2019 and build (or `ide/vs2017/mimalloc.sln`).
The `mimalloc` project builds a static library (in `out/msvc-x64`), while the
`mimalloc-override` project builds a DLL for overriding malloc
in the entire program.

## macOS, Linux, BSD, etc.

We use [`cmake`](https://cmake.org)<sup>1</sup> as the build system:

```
> mkdir -p out/release
> cd out/release
> cmake ../..
> make
```
This builds the library as a shared (dynamic)
library (`.so` or `.dylib`), a static library (`.a`), and
as a single object file (`.o`).

`> sudo make install` (install the library and header files in `/usr/local/lib`  and `/usr/local/include`)

You can build the debug version which does many internal checks and
maintains detailed statistics as:

```
> mkdir -p out/debug
> cd out/debug
> cmake -DCMAKE_BUILD_TYPE=Debug ../..
> make
```
This will name the shared library as `libmimalloc-debug.so`.

Finally, you can build a _secure_ version that uses guard pages, encrypted
free lists, etc., as:
```
> mkdir -p out/secure
> cd out/secure
> cmake -DMI_SECURE=ON ../..
> make
```
This will name the shared library as `libmimalloc-secure.so`.
Use `ccmake`<sup>2</sup> instead of `cmake`
to see and customize all the available build options.

Notes:
1. Install CMake: `sudo apt-get install cmake`
2. Install CCMake: `sudo apt-get install cmake-curses-gui`



# Using the library

The preferred usage is including `<mimalloc.h>`, linking with
the shared- or static library, and using the `mi_malloc` API exclusively for allocation. For example,
```
> gcc -o myprogram -lmimalloc myfile.c
```

mimalloc uses only safe OS calls (`mmap` and `VirtualAlloc`) and can co-exist
with other allocators linked to the same program.
If you use `cmake`, you can simply use:
```
find_package(mimalloc 1.0 REQUIRED)
```
in your `CMakeLists.txt` to find a locally installed mimalloc. Then use either:
```
target_link_libraries(myapp PUBLIC mimalloc)
```
to link with the shared (dynamic) library, or:
```
target_link_libraries(myapp PUBLIC mimalloc-static)
```
to link with the static library. See `test\CMakeLists.txt` for an example.

For best performance in C++ programs, it is also recommended to override the
global `new` and `delete` operators. For convience, mimalloc provides
[mimalloc-new-delete.h](https://github.com/microsoft/mimalloc/blob/master/include/mimalloc-new-delete.h) which does this for you -- just include it in a single(!) source file in your project.

You can pass environment variables to print verbose messages (`MIMALLOC_VERBOSE=1`)
and statistics (`MIMALLOC_SHOW_STATS=1`) (in the debug version):
```
> env MIMALLOC_SHOW_STATS=1 ./cfrac 175451865205073170563711388363

175451865205073170563711388363 = 374456281610909315237213 * 468551

heap stats:     peak      total      freed       unit
normal   2:    16.4 kb    17.5 mb    17.5 mb      16 b   ok
normal   3:    16.3 kb    15.2 mb    15.2 mb      24 b   ok
normal   4:      64 b      4.6 kb     4.6 kb      32 b   ok
normal   5:      80 b    118.4 kb   118.4 kb      40 b   ok
normal   6:      48 b       48 b       48 b       48 b   ok
normal  17:     960 b      960 b      960 b      320 b   ok

heap stats:     peak      total      freed       unit
    normal:    33.9 kb    32.8 mb    32.8 mb       1 b   ok
      huge:       0 b        0 b        0 b        1 b   ok
     total:    33.9 kb    32.8 mb    32.8 mb       1 b   ok
malloc requested:         32.8 mb

 committed:    58.2 kb    58.2 kb    58.2 kb       1 b   ok
  reserved:     2.0 mb     2.0 mb     2.0 mb       1 b   ok
     reset:       0 b        0 b        0 b        1 b   ok
  segments:       1          1          1
-abandoned:       0
     pages:       6          6          6
-abandoned:       0
     mmaps:       3
 mmap fast:       0
 mmap slow:       1
   threads:       0
   elapsed:     2.022s
   process: user: 1.781s, system: 0.016s, faults: 756, reclaims: 0, rss: 2.7 mb
```

The above model of using the `mi_` prefixed API is not always possible
though in existing programs that already use the standard malloc interface,
and another option is to override the standard malloc interface
completely and redirect all calls to the _mimalloc_ library instead.

## Environment Options

You can set further options either programmatically (using [`mi_option_set`](https://microsoft.github.io/mimalloc/group__options.html)),
or via environment variables.

- `MIMALLOC_SHOW_STATS=1`: show statistics when the program terminates.
- `MIMALLOC_VERBOSE=1`: show verbose messages.
- `MIMALLOC_SHOW_ERRORS=1`: show error and warning messages.
- `MIMALLOC_LARGE_OS_PAGES=1`: use large OS pages when available; for some workloads this can significantly
   improve performance. Use `MIMALLOC_VERBOSE` to check if the large OS pages are enabled -- usually one needs
   to explicitly allow large OS pages (as on [Windows][windows-huge] and [Linux][linux-huge]). However, sometimes
   the OS is very slow to reserve contiguous physical memory for large OS pages so use with care on systems that
   can have fragmented memory.
- `MIMALLOC_EAGER_REGION_COMMIT=1`: on Windows, commit large (256MiB) regions eagerly. On Windows, these regions
   show in the working set even though usually just a small part is committed to physical memory. This is why it
   turned off by default on Windows as it looks not good in the task manager. However, in reality it is always better
   to turn it on as it improves performance and has no other drawbacks.
- `MIMALLOC_RESERVE_HUGE_OS_PAGES=N`: where N is the number of 1GiB huge OS pages. This reserves the huge pages at
   startup and can give quite a performance improvement on long running workloads. Usually it is better to not use
   `MIMALLOC_LARGE_OS_PAGES` in combination with this setting. Just like large OS pages, use with care as reserving
   contiguous physical memory can take a long time when memory is fragmented. Still experimental.

[linux-huge]: https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/5/html/tuning_and_optimizing_red_hat_enterprise_linux_for_oracle_9i_and_10g_databases/sect-oracle_9i_and_10g_tuning_guide-large_memory_optimization_big_pages_and_huge_pages-configuring_huge_pages_in_red_hat_enterprise_linux_4_or_5
[windows-huge]: https://docs.microsoft.com/en-us/sql/database-engine/configure-windows/enable-the-lock-pages-in-memory-option-windows?view=sql-server-2017

# Overriding Malloc

Overriding the standard `malloc` can be done either _dynamically_ or _statically_.

## Dynamic override

This is the recommended way to override the standard malloc interface.

### Linux, BSD

On these ELF-based systems we preload the mimalloc shared
library so all calls to the standard `malloc` interface are
resolved to the _mimalloc_ library.
```
> env LD_PRELOAD=/usr/lib/libmimalloc.so myprogram
```

You can set extra environment variables to check that mimalloc is running,
like:
```
> env MIMALLOC_VERBOSE=1 LD_PRELOAD=/usr/lib/libmimalloc.so myprogram
```
or run with the debug version to get detailed statistics:
```
> env MIMALLOC_SHOW_STATS=1 LD_PRELOAD=/usr/lib/libmimalloc-debug.so myprogram
```

### MacOS

On macOS we can also preload the mimalloc shared
library so all calls to the standard `malloc` interface are
resolved to the _mimalloc_ library.
```
> env DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=/usr/lib/libmimalloc.dylib myprogram
```

Note that certain security restrictions may apply when doing this from
the [shell](https://stackoverflow.com/questions/43941322/dyld-insert-libraries-ignored-when-calling-application-through-bash).

Note: unfortunately, at this time, dynamic overriding on macOS seems broken but it is actively worked on to fix this
(see issue [`#50`](https://github.com/microsoft/mimalloc/issues/50)).

### Windows

On Windows you need to link your program explicitly with the mimalloc
DLL and use the C-runtime library as a DLL (using the `/MD` or `/MDd` switch).
Moreover, you need to ensure the `mimalloc-redirect.dll` (or `mimalloc-redirect32.dll`) is available
in the same folder as the main `mimalloc-override.dll` at runtime (as it is a dependency).
The redirection DLL ensures that all calls to the C runtime malloc API get redirected to
mimalloc (in `mimalloc-override.dll`).

To ensure the mimalloc DLL is loaded at run-time it is easiest to insert some
call to the mimalloc API in the `main` function, like `mi_version()`
(or use the `/INCLUDE:mi_version` switch on the linker). See the `mimalloc-override-test` project
for an example on how to use this. For best performance on Windows with C++, it
is highly recommended to also override the `new`/`delete` operations (as described
in the introduction).

The environment variable `MIMALLOC_DISABLE_REDIRECT=1` can be used to disable dynamic
overriding at run-time. Use `MIMALLOC_VERBOSE=1` to check if mimalloc was successfully redirected.

(Note: in principle, it is possible to patch existing executables
that are linked with the dynamic C runtime (`ucrtbase.dll`) by just putting the `mimalloc-override.dll` into the import table (and putting `mimalloc-redirect.dll` in the same folder)
Such patching can be done for example with [CFF Explorer](https://ntcore.com/?page_id=388)).


## Static override

On Unix-like systems, you can also statically link with _mimalloc_ to override the standard
malloc interface. The recommended way is to link the final program with the
_mimalloc_ single object file (`mimalloc-override.o`). We use
an object file instead of a library file as linkers give preference to
that over archives to resolve symbols. To ensure that the standard
malloc interface resolves to the _mimalloc_ library, link it as the first
object file. For example:
```
> gcc -o myprogram mimalloc-override.o  myfile1.c ...
```

Another way to override statically that works on all platforms, is to
link statically to mimalloc (as shown in the introduction) and include a
header file in each source file that re-defines `malloc` etc. to `mi_malloc`.
This is provided by [`mimalloc-override.h`](https://github.com/microsoft/mimalloc/blob/master/include/mimalloc-override.h). This only works reliably though if all sources are
under your control or otherwise mixing of pointers from different heaps may occur!


# Performance

We tested _mimalloc_ against many other top allocators over a wide
range of benchmarks, ranging from various real world programs to
synthetic benchmarks that see how the allocator behaves under more
extreme circumstances.

In our benchmarks, _mimalloc_ always outperforms all other leading
allocators (_jemalloc_, _tcmalloc_, _Hoard_, etc), and usually uses less
memory (up to 25% more in the worst case). A nice property is that it
does *consistently* well over the wide range of benchmarks.

Allocators are interesting as there exists no algorithm that is generally
optimal -- for a given allocator one can usually construct a workload
where it does not do so well. The goal is thus to find an allocation
strategy that performs well over a wide range of benchmarks without
suffering from underperformance in less common situations (which is what
the second half of our benchmark set tests for).

We show here only the results on an AMD EPYC system (Apr 2019) -- for
specific details and further benchmarks we refer to the [technical report](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action).

The benchmark suite is scripted and available separately
as [mimalloc-bench](https://github.com/daanx/mimalloc-bench).


## Benchmark Results

Testing on a big Amazon EC2 instance ([r5a.4xlarge](https://aws.amazon.com/ec2/instance-types/))
consisting of a 16-core AMD EPYC 7000 at 2.5GHz
with 128GB ECC memory, running	Ubuntu 18.04.1 with LibC 2.27 and GCC 7.3.0.
The measured allocators are _mimalloc_ (mi),
Google's [_tcmalloc_](https://github.com/gperftools/gperftools) (tc) used in Chrome,
[_jemalloc_](https://github.com/jemalloc/jemalloc) (je) by Jason Evans used in Firefox and FreeBSD,
[_snmalloc_](https://github.com/microsoft/snmalloc) (sn) by Liétar et al. \[8], [_rpmalloc_](https://github.com/rampantpixels/rpmalloc) (rp) by Mattias Jansson at Rampant Pixels,
[_Hoard_](https://github.com/emeryberger/Hoard) by Emery Berger \[1],
the system allocator (glibc) (based on _PtMalloc2_), and the Intel thread
building blocks [allocator](https://github.com/intel/tbb) (tbb).

![bench-r5a-1](doc/bench-r5a-1.svg)
![bench-r5a-2](doc/bench-r5a-2.svg)

Memory usage:

![bench-r5a-rss-1](doc/bench-r5a-rss-1.svg)
![bench-r5a-rss-1](doc/bench-r5a-rss-2.svg)

(note: the _xmalloc-testN_ memory usage should be disregarded as it
allocates more the faster the program runs).

In the first five benchmarks we can see _mimalloc_ outperforms the other
allocators moderately, but we also see that all these modern allocators
perform well -- the times of large performance differences in regular
workloads are over :-).
In _cfrac_ and _espresso_, _mimalloc_ is a tad faster than _tcmalloc_ and
_jemalloc_, but a solid 10\% faster than all other allocators on
_espresso_. The _tbb_ allocator does not do so well here and lags more than
20\% behind _mimalloc_. The _cfrac_ and _espresso_ programs do not use much
memory (~1.5MB) so it does not matter too much, but still _mimalloc_ uses
about half the resident memory of _tcmalloc_.

The _leanN_ program is most interesting as a large realistic and
concurrent workload of the [Lean](https://github.com/leanprover/lean) theorem prover
compiling its own standard library, and there is a 8% speedup over _tcmalloc_. This is
quite significant: if Lean spends 20% of its time in the
allocator that means that _mimalloc_ is 1.3&times; faster than _tcmalloc_
here. (This is surprising as that is not measured in a pure
allocation benchmark like _alloc-test_. We conjecture that we see this
outsized improvement here because _mimalloc_ has better locality in
the allocation which improves performance for the *other* computations
in a program as well).

The _redis_ benchmark shows more differences between the allocators where
_mimalloc_ is 14\% faster than _jemalloc_. On this benchmark _tbb_ (and _Hoard_) do
not do well and are over 40\% slower.

The _larson_ server workload allocates and frees objects between
many threads. Larson and Krishnan \[2] observe this
behavior (which they call _bleeding_) in actual server applications, and the
benchmark simulates this.
Here, _mimalloc_ is more than 2.5&times; faster than _tcmalloc_ and _jemalloc_
due to the object migration between different threads. This is a difficult
benchmark for other allocators too where _mimalloc_ is still 48% faster than the next
fastest (_snmalloc_).


The second benchmark set tests specific aspects of the allocators and
shows even more extreme differences between them.

The _alloc-test_, by
[OLogN Technologies AG](http://ithare.com/testing-memory-allocators-ptmalloc2-tcmalloc-hoard-jemalloc-while-trying-to-simulate-real-world-loads/), is a very allocation intensive benchmark doing millions of
allocations in various size classes. The test is scaled such that when an
allocator performs almost identically on _alloc-test1_ as _alloc-testN_ it
means that it scales linearly. Here, _tcmalloc_, _snmalloc_, and
_Hoard_ seem to scale less well and do more than 10% worse on the
multi-core version. Even the best allocators (_tcmalloc_ and _jemalloc_) are
more than 10% slower as _mimalloc_ here.

The _sh6bench_ and _sh8bench_ benchmarks are
developed by [MicroQuill](http://www.microquill.com/) as part of SmartHeap.
In _sh6bench_ _mimalloc_ does much
better than the others (more than 2&times; faster than _jemalloc_).
We cannot explain this well but believe it is
caused in part by the "reverse" free-ing pattern in _sh6bench_.
Again in _sh8bench_ the _mimalloc_ allocator handles object migration
between threads much better and is over 36% faster than the next best
allocator, _snmalloc_. Whereas _tcmalloc_ did well on _sh6bench_, the
addition of object migration caused it to be almost 3 times slower
than before.

The _xmalloc-testN_ benchmark by Lever and Boreham \[5] and Christian Eder,
simulates an asymmetric workload where
some threads only allocate, and others only free. The _snmalloc_
allocator was especially developed to handle this case well as it
often occurs in concurrent message passing systems (like the [Pony] language
for which _snmalloc_ was initially developed). Here we see that
the _mimalloc_ technique of having non-contended sharded thread free
lists pays off as it even outperforms _snmalloc_ here.
Only _jemalloc_ also handles this reasonably well, while the
others underperform by a large margin.

The _cache-scratch_ benchmark by Emery Berger \[1], and introduced with the Hoard
allocator to test for _passive-false_ sharing of cache lines. With a single thread they all
perform the same, but when running with multiple threads the potential allocator
induced false sharing of the cache lines causes large run-time
differences, where _mimalloc_ is more than 18&times; faster than _jemalloc_ and
_tcmalloc_! Crundal \[6] describes in detail why the false cache line
sharing occurs in the _tcmalloc_ design, and also discusses how this
can be avoided with some small implementation changes.
Only _snmalloc_ and _tbb_ also avoid the
cache line sharing like _mimalloc_. Kukanov and Voss \[7] describe in detail
how the design of _tbb_ avoids the false cache line sharing.



# References

- \[1] Emery D. Berger, Kathryn S. McKinley, Robert D. Blumofe, and Paul R. Wilson.
   _Hoard: A Scalable Memory Allocator for Multithreaded Applications_
   the Ninth International Conference on Architectural Support for Programming Languages and Operating Systems (ASPLOS-IX). Cambridge, MA, November 2000.
   [pdf](http://www.cs.utexas.edu/users/mckinley/papers/asplos-2000.pdf)


- \[2] P. Larson and M. Krishnan. _Memory allocation for long-running server applications_. In ISMM, Vancouver, B.C., Canada, 1998.
      [pdf](http://citeseer.ist.psu.edu/viewdoc/download;jsessionid=5F0BFB4F57832AEB6C11BF8257271088?doi=10.1.1.45.1947&rep=rep1&type=pdf)

- \[3] D. Grunwald, B. Zorn, and R. Henderson.
  _Improving the cache locality of memory allocation_. In R. Cartwright, editor,
  Proceedings of the Conference on Programming Language Design and Implementation, pages 177–186, New York, NY, USA, June 1993.
  [pdf](http://citeseer.ist.psu.edu/viewdoc/download?doi=10.1.1.43.6621&rep=rep1&type=pdf)

- \[4] J. Barnes and P. Hut. _A hierarchical O(n*log(n)) force-calculation algorithm_. Nature, 324:446-449, 1986.

- \[5] C. Lever, and D. Boreham. _Malloc() Performance in a Multithreaded Linux Environment._
  In USENIX Annual Technical Conference, Freenix Session. San Diego, CA. Jun. 2000.
  Available at <https://github.com/kuszmaul/SuperMalloc/tree/master/tests>

- \[6] Timothy Crundal. _Reducing Active-False Sharing in TCMalloc._
   2016. <http://courses.cecs.anu.edu.au/courses/CSPROJECTS/16S1/Reports/Timothy_Crundal_Report.pdf>. CS16S1 project at the Australian National University.

- \[7] Alexey Kukanov, and Michael J Voss.
   _The Foundations for Scalable Multi-Core Software in Intel Threading Building Blocks._
   Intel Technology Journal 11 (4). 2007

- \[8] Paul Liétar, Theodore Butler, Sylvan Clebsch, Sophia Drossopoulou, Juliana Franco, Matthew J Parkinson,
  Alex Shamis, Christoph M Wintersteiger, and David Chisnall.
  _Snmalloc: A Message Passing Allocator._
  In Proceedings of the 2019 ACM SIGPLAN International Symposium on Memory Management, 122–135. ACM. 2019.


# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.
