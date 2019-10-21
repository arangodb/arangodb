+++
title = "Prerequisites"
weight = 2
+++

Outcome is a header-only C++ 14 library known to work well on the latest
point releases of these compiler-platform combinations or better:

- clang 4.0.1 (LLVM) [FreeBSD, Linux, OS X]
- GCC 6.5 [Linux]
- Visual Studio 2017.9 [Windows]
- XCode 9 [MacOS]

It is worth turning on C++ 17 if you can, as there are many usability and
performance improvements. If your compiler implements the Concepts TS, it
is worth turning support on. Support is automatically
detected and used.


Partially working compilers (this was last updated January 2019):

- clang 3.5 - 3.9 can compile varying degrees of the test suite, the
problem is lack of complete and unbuggy C++ 14 language support.
- Older point releases of GCCs 7 and 8 have internal compiler error bugs
in their constexpr implementation which tend to be triggered by using
Outcome in constexpr. If you don't use Outcome in constexpr, you won't
see these problems. If you need your GCC to not ICE, upgrade to the
very latest point release, the constexpr ICE has been since fixed.
- Early editions of Visual Studio 2017 have many corner case problems.
The latest point release, VS2017.9, only has a few known problems,
and should be relatively unsurprising for most use cases.

---

"C++ 14" compilers which do not work, and will not work until their
maintainers fix them:

- GCC 5, due to a bug in nested template variables parsing which was fixed
in GCC 6. I appreciate that this upsets a lot of users. Please raise your
upset at https://gcc.gnu.org/bugzilla/. In the meantime, you can get fairly
far in Outcome with even clang 3.5.
- Any compiler which uses the libstdc++ version which comes with GCC 5, as it does
not implement enough of the C++ 14 standard library for Outcome to compile.
