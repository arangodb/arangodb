use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  # When changing these, also update the svn revisions in deps_revisions.gni
  "clang_format_revision": "96636aa0e9f047f17447f2d45a094d0b59ed7917",
  "libcxx_revision":       "2199647acb904b91eea0a5e045f5b227c87d6e85",
  "libcxxabi_revision":    "c3f4753f7139c73063304235781e4f7788a94c06",
  "libunwind_revision":    "94edd59b16d2084a62699290f9cfdf120d74eedf",
}

deps = {
  "clang_format/script":
    Var("chromium_url") + "/chromium/llvm-project/cfe/tools/clang-format.git@" +
    Var("clang_format_revision"),
  "third_party/libc++/trunk":
    Var("chromium_url") + "/chromium/llvm-project/libcxx.git" + "@" +
    Var("libcxx_revision"),
  "third_party/libc++abi/trunk":
    Var("chromium_url") + "/chromium/llvm-project/libcxxabi.git" + "@" +
    Var("libcxxabi_revision"),
  "third_party/libunwind/trunk":
    Var("chromium_url") + "/external/llvm.org/libunwind.git" + "@" +
    Var("libunwind_revision"),
}
