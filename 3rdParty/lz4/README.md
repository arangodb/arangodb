LZ4 - Extremely fast compression
================================

LZ4 is lossless compression algorithm,
providing compression speed at 400 MB/s per core,
scalable with multi-cores CPU.
It features an extremely fast decoder,
with speed in multiple GB/s per core,
typically reaching RAM speed limits on multi-core systems.

Speed can be tuned dynamically, selecting an "acceleration" factor
which trades compression ratio for more speed up.
On the other end, a high compression derivative, LZ4_HC, is also provided,
trading CPU time for improved compression ratio.
All versions feature the same decompression speed.

LZ4 library is provided as open-source software using BSD 2-Clause license.


|Branch      |Status   |
|------------|---------|
|master      | [![Build Status][travisMasterBadge]][travisLink] [![Build status][AppveyorMasterBadge]][AppveyorLink] [![coverity][coverBadge]][coverlink] |
|dev         | [![Build Status][travisDevBadge]][travisLink]    [![Build status][AppveyorDevBadge]][AppveyorLink]                                         |

[travisMasterBadge]: https://travis-ci.org/lz4/lz4.svg?branch=master "Continuous Integration test suite"
[travisDevBadge]: https://travis-ci.org/lz4/lz4.svg?branch=dev "Continuous Integration test suite"
[travisLink]: https://travis-ci.org/lz4/lz4
[AppveyorMasterBadge]: https://ci.appveyor.com/api/projects/status/github/lz4/lz4?branch=master&svg=true "Windows test suite"
[AppveyorDevBadge]: https://ci.appveyor.com/api/projects/status/github/lz4/lz4?branch=dev&svg=true "Windows test suite"
[AppveyorLink]: https://ci.appveyor.com/project/YannCollet/lz4-1lndh
[coverBadge]: https://scan.coverity.com/projects/4735/badge.svg "Static code analysis of Master branch"
[coverlink]: https://scan.coverity.com/projects/4735

> **Branch Policy:**
> - The "master" branch is considered stable, at all times.
> - The "dev" branch is the one where all contributions must be merged
    before being promoted to master.
>   + If you plan to propose a patch, please commit into the "dev" branch,
      or its own feature branch.
      Direct commit to "master" are not permitted.

Benchmarks
-------------------------

The benchmark uses [lzbench], from @inikep
compiled with GCC v6.2.0 on Linux 64-bits.
The reference system uses a Core i7-3930K CPU @ 4.5GHz.
Benchmark evaluates the compression of reference [Silesia Corpus]
in single-thread mode.

[lzbench]: https://github.com/inikep/lzbench
[Silesia Corpus]: http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia

|  Compressor            | Ratio   | Compression | Decompression |
|  ----------            | -----   | ----------- | ------------- |
|  memcpy                |  1.000  | 7300 MB/s   |   7300 MB/s   |
|**LZ4 fast 8  (v1.7.3)**|  1.799  |**911 MB/s** | **3360 MB/s** |
|**LZ4 default (v1.7.3)**|**2.101**|**625 MB/s** | **3220 MB/s** |
|  LZO 2.09              |  2.108  |  620 MB/s   |    845 MB/s   |
|  QuickLZ 1.5.0         |  2.238  |  510 MB/s   |    600 MB/s   |
|  Snappy 1.1.3          |  2.091  |  450 MB/s   |   1550 MB/s   |
|  LZF v3.6              |  2.073  |  365 MB/s   |    820 MB/s   |
|  [Zstandard] 1.1.1 -1  |  2.876  |  330 MB/s   |    930 MB/s   |
|  [Zstandard] 1.1.1 -3  |  3.164  |  200 MB/s   |    810 MB/s   |
| [zlib] deflate 1.2.8 -1|  2.730  |  100 MB/s   |    370 MB/s   |
|**LZ4 HC -9 (v1.7.3)**  |**2.720**|   34 MB/s   | **3240 MB/s** |
| [zlib] deflate 1.2.8 -6|  3.099  |   33 MB/s   |    390 MB/s   |

[zlib]: http://www.zlib.net/
[Zstandard]: http://www.zstd.net/

LZ4 is also compatible and well optimized for x32 mode, for which it provides an additional +10% speed performance.


Installation
-------------------------

```
make
make install     # this command may require root access
```

LZ4's `Makefile` supports standard [Makefile conventions],
including [staged installs], [redirection], or [command redefinition].
It is compatible with parallel builds (`-j#`).

[Makefile conventions]: https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html
[staged installs]: https://www.gnu.org/prep/standards/html_node/DESTDIR.html
[redirection]: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html
[command redefinition]: https://www.gnu.org/prep/standards/html_node/Utilities-in-Makefiles.html


Documentation
-------------------------

The raw LZ4 block compression format is detailed within [lz4_Block_format].

To compress an arbitrarily long file or data stream, multiple blocks are required.
Organizing these blocks and providing a common header format to handle their content
is the purpose of the Frame format, defined into [lz4_Frame_format].
Interoperable versions of LZ4 must respect this frame format.

[lz4_Block_format]: doc/lz4_Block_format.md
[lz4_Frame_format]: doc/lz4_Frame_format.md


Other source versions
-------------------------

Beyond the C reference source,
many contributors have created versions of lz4 in multiple languages
(Java, C#, Python, Perl, Ruby, etc.).
A list of known source ports is maintained on the [LZ4 Homepage].

[LZ4 Homepage]: http://www.lz4.org
