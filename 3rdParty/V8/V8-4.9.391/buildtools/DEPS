recursion = 1
use_relative_paths = True

vars = {
  "git_url": "https://chromium.googlesource.com",

  "clang_format_rev": "81edd558fea5dd7855d67a1dc61db34ae8c1fd63", # r223685
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
