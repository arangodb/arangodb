load("@bazel_skylib//lib:selects.bzl", "selects")
load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

# Detect compiler:
config_setting(
    name = "compiler_clang",
    flag_values = {"@bazel_tools//tools/cpp:compiler": "clang"},
)

config_setting(
    name = "compiler_msvc",
    flag_values = {"@bazel_tools//tools/cpp:compiler": "msvc"},
)

# See https://github.com/bazelbuild/bazel/issues/12707
config_setting(
    name = "compiler_gcc_bug",
    flag_values = {
        "@bazel_tools//tools/cpp:compiler": "compiler",
    },
)

config_setting(
    name = "compiler_gcc_actual",
    flag_values = {
        "@bazel_tools//tools/cpp:compiler": "gcc",
    },
)

selects.config_setting_group(
    name = "compiler_gcc",
    match_any = [
        ":compiler_gcc_bug",
        ":compiler_gcc_actual",
    ],
)

config_setting(
    name = "emulate_sve",
    values = {
        "copt": "-DHWY_EMULATE_SVE",
    },
)

# Additional warnings for Clang OR GCC (skip for MSVC)
CLANG_GCC_COPTS = [
    "-Werror",
    "-Wunused-parameter",
    "-Wunused-variable",
    "-Wextra-semi",
    "-Wunreachable-code",
]

# Additional warnings only supported by Clang
CLANG_ONLY_COPTS = [
    "-Wfloat-overflow-conversion",
    "-Wfloat-zero-conversion",
    "-Wfor-loop-analysis",
    "-Wgnu-redeclared-enum",
    "-Winfinite-recursion",
    "-Wliteral-conversion",
    "-Wno-c++98-compat",
    "-Wno-unused-command-line-argument",
    "-Wprivate-header",
    "-Wself-assign",
    "-Wstring-conversion",
    "-Wtautological-overlap-compare",
    "-Wthread-safety-analysis",
    "-Wundefined-func-template",
    "-Wunused-comparison",
]

COPTS = select({
    ":compiler_msvc": [],
    ":compiler_gcc": CLANG_GCC_COPTS,
    # Default to clang because compiler detection only works in Bazel
    "//conditions:default": CLANG_GCC_COPTS + CLANG_ONLY_COPTS,
})

# Unused on Bazel builds, where this is not defined/known; Copybara replaces
# usages with an empty list.
COMPAT = [
    "//buildenv/target:non_prod",  # includes mobile/vendor.
]

# WARNING: changing flags such as HWY_DISABLED_TARGETS may break users without
# failing integration tests, if the machine running tests does not support the
# newly enabled instruction set, or the failure is only caught by sanitizers
# which do not run in CI.

cc_library(
    name = "hwy",
    srcs = [
        "hwy/aligned_allocator.cc",
        "hwy/targets.cc",
    ],
    # Normal headers with include guards
    hdrs = [
        "hwy/aligned_allocator.h",
        "hwy/base.h",
        "hwy/cache_control.h",
        "hwy/detect_compiler_arch.h",  # private
        "hwy/detect_targets.h",  # private
        "hwy/targets.h",
    ],
    compatible_with = [],
    copts = COPTS,
    textual_hdrs = [
        "hwy/highway.h",  # public
        "hwy/foreach_target.h",  # public
        "hwy/ops/arm_neon-inl.h",
        "hwy/ops/arm_sve-inl.h",
        "hwy/ops/generic_ops-inl.h",
        "hwy/ops/rvv-inl.h",
        "hwy/ops/scalar-inl.h",
        "hwy/ops/set_macros-inl.h",
        "hwy/ops/shared-inl.h",
        "hwy/ops/wasm_128-inl.h",
        "hwy/ops/x86_128-inl.h",
        "hwy/ops/x86_256-inl.h",
        "hwy/ops/x86_512-inl.h",
    ],
    deps = select({
        ":emulate_sve": ["//third_party/farm_sve"],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "image",
    srcs = [
        "hwy/contrib/image/image.cc",
    ],
    hdrs = [
        "hwy/contrib/image/image.h",
    ],
    compatible_with = [],
    deps = [":hwy"],
)

cc_library(
    name = "math",
    compatible_with = [],
    textual_hdrs = [
        "hwy/contrib/math/math-inl.h",
    ],
    deps = [":hwy"],
)

cc_library(
    name = "hwy_test_util",
    textual_hdrs = ["hwy/tests/test_util-inl.h"],
    deps = [":hwy"],
)

cc_library(
    name = "nanobenchmark",
    srcs = ["hwy/nanobenchmark.cc"],
    hdrs = ["hwy/nanobenchmark.h"],
    deps = [":hwy"],
)

cc_binary(
    name = "benchmark",
    srcs = ["hwy/examples/benchmark.cc"],
    deps = [
        ":hwy",
        ":nanobenchmark",
    ],
)

cc_library(
    name = "skeleton",
    srcs = ["hwy/examples/skeleton.cc"],
    hdrs = ["hwy/examples/skeleton.h"],
    textual_hdrs = ["hwy/examples/skeleton-inl.h"],
    deps = [":hwy"],
)

cc_binary(
    name = "list_targets",
    srcs = ["hwy/tests/list_targets.cc"],
    deps = [":hwy"],
)

# path, name
HWY_TESTS = [
    ("hwy/contrib/image/", "image_test"),
    ("hwy/contrib/math/", "math_test"),
    ("hwy/examples/", "skeleton_test"),
    ("hwy/", "nanobenchmark_test"),
    ("hwy/", "aligned_allocator_test"),
    ("hwy/", "base_test"),
    ("hwy/", "highway_test"),
    ("hwy/", "targets_test"),
    ("hwy/tests/", "arithmetic_test"),
    ("hwy/tests/", "blockwise_test"),
    ("hwy/tests/", "combine_test"),
    ("hwy/tests/", "compare_test"),
    ("hwy/tests/", "convert_test"),
    ("hwy/tests/", "crypto_test"),
    ("hwy/tests/", "logical_test"),
    ("hwy/tests/", "mask_test"),
    ("hwy/tests/", "memory_test"),
    ("hwy/tests/", "swizzle_test"),
    ("hwy/tests/", "test_util_test"),
]

[
    [
        cc_test(
            name = test,
            size = "medium",
            timeout = "long",  # default moderate is not enough for math_test
            srcs = [
                subdir + test + ".cc",
            ],
            copts = COPTS + [
                # gTest triggers this warning (which is enabled by the
                # extra-semi in COPTS), so we need to disable it here,
                # but it's still enabled for :hwy.
                "-Wno-c++98-compat-extra-semi",
            ],
            # for test_suite.
            tags = ["hwy_ops_test"],
            deps = [
                ":hwy",
                ":hwy_test_util",
                ":image",
                ":math",
                ":nanobenchmark",
                ":skeleton",
                "@com_google_googletest//:gtest_main",
            ],
        ),
    ]
    for subdir, test in HWY_TESTS
]

# For manually building the tests we define here (:all does not work in --config=msvc)
test_suite(
    name = "hwy_ops_tests",
    tags = ["hwy_ops_test"],
)
