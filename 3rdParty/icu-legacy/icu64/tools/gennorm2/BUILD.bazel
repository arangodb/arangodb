# © 2021 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# This Bazel build file defines a target for the gennorm2 binary that generates
# headers needed for bootstrapping the ICU4C build process in a way that
# integrates the normalization data.

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_binary(
    name = "gennorm2",
    srcs = glob([
        "*.c",
        "*.cpp",
        "*.h",   # cannot have hdrs section in cc_binary
    ]),
    deps = [
        "//icu4c/source/common:uhash",
        "//icu4c/source/common:umutablecptrie",
        "//icu4c/source/common:ucptrie",
        "//icu4c/source/common:errorcode",
        "//icu4c/source/common:uniset",
        "//icu4c/source/common:uvector32",

        "//icu4c/source/common:platform",
        "//icu4c/source/common:headers",
        
        "//icu4c/source/tools/toolutil:toolutil",
        "//icu4c/source/tools/toolutil:unewdata",
        "//icu4c/source/tools/toolutil:writesrc",
        "//icu4c/source/tools/toolutil:uoptions",
        "//icu4c/source/tools/toolutil:uparse",
    ],
    linkopts = ["-pthread"],
)
