Build and tests passing: Linux [![Build Status](https://travis-ci.org/boostorg/outcome.svg?branch=master)](https://travis-ci.org/boostorg/outcome)

Documentation generated from the master branch shown below (may be newer than boost.org's): http://boostorg.github.io/outcome

This is the Boost-ified edition of standalone https://github.com/ned14/outcome.
Every commit made to standalone Outcome (develop and master branches only) gets
automatically converted and merged to here.

Note that changes made here may not get noticed in a timely fashion.
Please try to send pull requests to https://github.com/ned14/outcome/pulls instead.

Similarly, please report bugs to https://github.com/ned14/outcome/issues.

This library works well in older Boosts missing Outcome. Installation into an
older Boost and running the test suite would be as follows:

```
cd boost/libs
git clone --depth 1 https://github.com/boostorg/outcome.git outcome
cd ..
./b2 headers
./b2 libs/outcome/test cxxflags=--std=c++14
```
