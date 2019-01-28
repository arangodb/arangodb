from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
rocksdb_target_header = """load("@fbcode_macros//build_defs:auto_headers.bzl", "AutoHeaders")

REPO_PATH = package_name() + "/"

BUCK_BINS = "buck-out/gen/" + REPO_PATH

TEST_RUNNER = REPO_PATH + "buckifier/rocks_test_runner.sh"

rocksdb_compiler_flags = [
    "-fno-builtin-memcmp",
    "-DROCKSDB_PLATFORM_POSIX",
    "-DROCKSDB_LIB_IO_POSIX",
    "-DROCKSDB_FALLOCATE_PRESENT",
    "-DROCKSDB_MALLOC_USABLE_SIZE",
    "-DROCKSDB_RANGESYNC_PRESENT",
    "-DROCKSDB_SCHED_GETCPU_PRESENT",
    "-DROCKSDB_SUPPORT_THREAD_LOCAL",
    "-DOS_LINUX",
    # Flags to enable libs we include
    "-DSNAPPY",
    "-DZLIB",
    "-DBZIP2",
    "-DLZ4",
    "-DZSTD",
    "-DGFLAGS=gflags",
    "-DNUMA",
    "-DTBB",
    # Needed to compile in fbcode
    "-Wno-expansion-to-defined",
    # Added missing flags from output of build_detect_platform
    "-DROCKSDB_PTHREAD_ADAPTIVE_MUTEX",
    "-DROCKSDB_BACKTRACE",
    "-Wnarrowing",
]

rocksdb_external_deps = [
    ("bzip2", None, "bz2"),
    ("snappy", None, "snappy"),
    ("zlib", None, "z"),
    ("gflags", None, "gflags"),
    ("lz4", None, "lz4"),
    ("zstd", None),
    ("tbb", None),
    ("numa", None, "numa"),
    ("googletest", None, "gtest"),
]

rocksdb_preprocessor_flags = [
    # Directories with files for #include
    "-I" + REPO_PATH + "include/",
    "-I" + REPO_PATH,
]

rocksdb_arch_preprocessor_flags = {
    "x86_64": [
        "-DHAVE_SSE42",
        "-DHAVE_PCLMUL",
    ],
}

build_mode = read_config("fbcode", "build_mode")

is_opt_mode = build_mode.startswith("opt")

# -DNDEBUG is added by default in opt mode in fbcode. But adding it twice
# doesn't harm and avoid forgetting to add it.
if is_opt_mode:
    rocksdb_compiler_flags.append("-DNDEBUG")

default_allocator = read_config("fbcode", "default_allocator")

sanitizer = read_config("fbcode", "sanitizer")

# Let RocksDB aware of jemalloc existence.
# Do not enable it if sanitizer presents.
if is_opt_mode and default_allocator.startswith("jemalloc") and sanitizer == "":
    rocksdb_compiler_flags.append("-DROCKSDB_JEMALLOC")
    rocksdb_external_deps.append(("jemalloc", None, "headers"))
"""


library_template = """
cpp_library(
    name = "{name}",
    srcs = [{srcs}],
    {headers_attr_prefix}headers = {headers},
    arch_preprocessor_flags = rocksdb_arch_preprocessor_flags,
    compiler_flags = rocksdb_compiler_flags,
    preprocessor_flags = rocksdb_preprocessor_flags,
    deps = [{deps}],
    external_deps = rocksdb_external_deps,
)
"""

binary_template = """
cpp_binary(
    name = "%s",
    srcs = [%s],
    arch_preprocessor_flags = rocksdb_arch_preprocessor_flags,
    compiler_flags = rocksdb_compiler_flags,
    preprocessor_flags = rocksdb_preprocessor_flags,
    deps = [%s],
    external_deps = rocksdb_external_deps,
)
"""

test_cfg_template = """    [
        "%s",
        "%s",
        "%s",
    ],
"""

unittests_template = """
# [test_name, test_src, test_type]
ROCKS_TESTS = [
%s]

# Generate a test rule for each entry in ROCKS_TESTS
# Do not build the tests in opt mode, since SyncPoint and other test code
# will not be included.
if not is_opt_mode:
    for test_cfg in ROCKS_TESTS:
        test_name = test_cfg[0]
        test_cc = test_cfg[1]
        ttype = "gtest" if test_cfg[2] == "parallel" else "simple"
        test_bin = test_name + "_bin"

        cpp_binary(
            name = test_bin,
            srcs = [test_cc],
            arch_preprocessor_flags = rocksdb_arch_preprocessor_flags,
            compiler_flags = rocksdb_compiler_flags,
            preprocessor_flags = rocksdb_preprocessor_flags,
            deps = [":rocksdb_test_lib"],
            external_deps = rocksdb_external_deps,
        )

        custom_unittest(
            name = test_name,
            command = [TEST_RUNNER, BUCK_BINS + test_bin],
            type = ttype,
            deps = [":" + test_bin],
        )
"""
