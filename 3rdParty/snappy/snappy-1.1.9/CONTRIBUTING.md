# How to Contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Project Goals

In addition to the aims listed at the top of the [README](README.md) Snappy
explicitly supports the following:

1. C++11
2. Clang (gcc and MSVC are best-effort).
3. Low level optimizations (e.g. assembly or equivalent intrinsics) for:
  1. [x86](https://en.wikipedia.org/wiki/X86)
  2. [x86-64](https://en.wikipedia.org/wiki/X86-64)
  3. ARMv7 (32-bit)
  4. ARMv8 (AArch64)
4. Supports only the Snappy compression scheme as described in
  [format_description.txt](format_description.txt).
5. CMake for building

Changes adding features or dependencies outside of the core area of focus listed
above might not be accepted. If in doubt post a message to the
[Snappy discussion mailing list](https://groups.google.com/g/snappy-compression).

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution,
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult
[GitHub Help](https://help.github.com/articles/about-pull-requests/) for more
information on using pull requests.

Please make sure that all the automated checks (CLA, AppVeyor, Travis) pass for
your pull requests. Pull requests whose checks fail may be ignored.
