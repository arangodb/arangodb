## Release testing process

We run the following before a release:

### Windows x86 host

```
run_tests.bat
```

### Linux x86 host

Clang, GCC; Arm, PPC cross-compile: `./run_tests.sh`

Manual test of WASM and WASM_EMU256 targets.

Check libjxl build actions at https://github.com/libjxl/libjxl/pull/2269.

### Signing the release

*   Download release source code archive
*   `gpg --armor --detach-sign highway-1.0.4.tar.gz`
*   Edit release and attach the resulting `highway-1.0.4.tar.gz.asc`

(See https://wiki.debian.org/Creating%20signed%20GitHub%20releases)
