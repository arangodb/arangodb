to install

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

To build the tests, define the root of the gtest installation, e.g.

```
cmake .. -DGTEST_ROOT=/path/to/gtest
```

Test binaries have the suffix `_test`.
