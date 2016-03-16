recursion = 1
use_relative_paths = True

vars = {
  "git_url": "https://chromium.googlesource.com",

  "clang_format_rev": "a72164df8be7d1c68ae1ad6c3541e7819200327e", # r258123
  "libcxx_revision": "aad34a13af010898f54c1bb2069194cb083cea4b",
  "libcxxabi_revision": "9a39e428d018b723d7d187181fd08908b1cb6bd0",
}

deps = {
  "clang_format/script":
      Var("git_url") + "/chromium/llvm-project/cfe/tools/clang-format.git@" +
      Var("clang_format_rev"),
  "third_party/libc++/trunk":
      Var("git_url") + "/chromium/llvm-project/libcxx.git" + "@" +
      Var("libcxx_revision"),
  "third_party/libc++abi/trunk":
      Var("git_url") + "/chromium/llvm-project/libcxxabi.git" + "@" +
      Var("libcxxabi_revision"),
}
