# benchmark1.cpp results

## VS 2017 15.9.7 64 bit (cl.exe 19.16, /EHsc /std:c++17)

### /Od

#### Compile time

```
      variant2 (-DONLY_V2): 1837 ms
boost::variant (-DONLY_BV): 2627 ms
  std::variant (-DONLY_SV): 1425 ms
```

#### Run time

```
N=100000000:
        double:   9041 ms; S=7.14286e+14
      variant2:  48367 ms; S=7.14286e+14
boost::variant: 102776 ms; S=7.14286e+14
  std::variant:  40590 ms; S=7.14286e+14

N=100000000:
        double:   9029 ms; S=7.14286e+14
      variant2:  92962 ms; S=7.14286e+14
boost::variant: 110441 ms; S=7.14286e+14
  std::variant:  92974 ms; S=7.14286e+14
```

### /O2 /DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 2571 ms
boost::variant (-DONLY_BV): 3335 ms
  std::variant (-DONLY_SV): 1903 ms
```

#### Run time

```
N=100000000:
        double:   1949 ms; S=7.14286e+14
      variant2:   4176 ms; S=7.14286e+14
boost::variant:  11312 ms; S=7.14286e+14
  std::variant:   4617 ms; S=7.14286e+14

N=100000000:
        double:   1949 ms; S=7.14286e+14
      variant2:  11807 ms; S=7.14286e+14
boost::variant:  15632 ms; S=7.14286e+14
  std::variant:  10725 ms; S=7.14286e+14
```

## g++ 7.4.0 -std=c++17 (Cygwin 64 bit)

### -O0

#### Compile time

```
      variant2 (-DONLY_V2): 2734 ms
boost::variant (-DONLY_BV): 4308 ms
  std::variant (-DONLY_SV): 2298 ms
```

#### Run time

```
N=100000000:
        double:   3620 ms; S=7.14286e+14
      variant2:  29214 ms; S=7.14286e+14
boost::variant:  88492 ms; S=7.14286e+14
  std::variant:  39510 ms; S=7.14286e+14

N=100000000:
        double:   3642 ms; S=7.14286e+14
      variant2:  75822 ms; S=7.14286e+14
boost::variant:  96680 ms; S=7.14286e+14
  std::variant:  66411 ms; S=7.14286e+14
```

### -O1

#### Compile time

```
      variant2 (-DONLY_V2): 2103 ms
boost::variant (-DONLY_BV): 3398 ms
  std::variant (-DONLY_SV): 1841 ms
```

#### Run time

```
N=100000000:
        double:   1576 ms; S=7.14286e+14
      variant2:   3424 ms; S=7.14286e+14
boost::variant:   4356 ms; S=7.14286e+14
  std::variant:   3764 ms; S=7.14286e+14

N=100000000:
        double:   1582 ms; S=7.14286e+14
      variant2:   9062 ms; S=7.14286e+14
boost::variant:   9603 ms; S=7.14286e+14
  std::variant:   8825 ms; S=7.14286e+14
```

### -O2 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 2276 ms
boost::variant (-DONLY_BV): 3647 ms
  std::variant (-DONLY_SV): 2111 ms
```

#### Run time

```
N=100000000:
        double:   1643 ms; S=7.14286e+14
      variant2:   3070 ms; S=7.14286e+14
boost::variant:   3385 ms; S=7.14286e+14
  std::variant:   3880 ms; S=7.14286e+14

N=100000000:
        double:   1622 ms; S=7.14286e+14
      variant2:   8101 ms; S=7.14286e+14
boost::variant:   8611 ms; S=7.14286e+14
  std::variant:   8694 ms; S=7.14286e+14
```

### -O3 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 2390 ms
boost::variant (-DONLY_BV): 3768 ms
  std::variant (-DONLY_SV): 2094 ms
```

#### Run time

```
N=100000000:
        double:   1611 ms; S=7.14286e+14
      variant2:   2975 ms; S=7.14286e+14
boost::variant:   3232 ms; S=7.14286e+14
  std::variant:   3726 ms; S=7.14286e+14

N=100000000:
        double:   1603 ms; S=7.14286e+14
      variant2:   8157 ms; S=7.14286e+14
boost::variant:   8419 ms; S=7.14286e+14
  std::variant:   8659 ms; S=7.14286e+14
```

## clang++ 5.0.1 -std=c++17 -stdlib=libc++ (Cygwin 64 bit)

### -O0

#### Compile time

```
      variant2 (-DONLY_V2): 2190 ms
boost::variant (-DONLY_BV): 3537 ms
  std::variant (-DONLY_SV): 2151 ms
```

#### Run time

```
N=100000000:
        double:   6063 ms; S=7.14286e+14
      variant2:  23616 ms; S=7.14286e+14
boost::variant:  92730 ms; S=7.14286e+14
  std::variant:  23160 ms; S=7.14286e+14

N=100000000:
        double:   6054 ms; S=7.14286e+14
      variant2:  52738 ms; S=7.14286e+14
boost::variant:  96896 ms; S=7.14286e+14
  std::variant:  72595 ms; S=7.14286e+14
```

### -O1

#### Compile time

```
      variant2 (-DONLY_V2): 2722 ms
boost::variant (-DONLY_BV): 4337 ms
  std::variant (-DONLY_SV): 2697 ms
```

#### Run time

```
N=100000000:
        double:   2171 ms; S=7.14286e+14
      variant2:   9280 ms; S=7.14286e+14
boost::variant:  51478 ms; S=7.14286e+14
  std::variant:   5642 ms; S=7.14286e+14

N=100000000:
        double:   2171 ms; S=7.14286e+14
      variant2:  22166 ms; S=7.14286e+14
boost::variant:  54084 ms; S=7.14286e+14
  std::variant:  14330 ms; S=7.14286e+14
```

### -O2 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 2499 ms
boost::variant (-DONLY_BV): 3826 ms
  std::variant (-DONLY_SV): 2645 ms
```

#### Run time

```
N=100000000:
        double:   1604 ms; S=7.14286e+14
      variant2:   2726 ms; S=7.14286e+14
boost::variant:   6662 ms; S=7.14286e+14
  std::variant:   3869 ms; S=7.14286e+14

N=100000000:
        double:   1598 ms; S=7.14286e+14
      variant2:   8136 ms; S=7.14286e+14
boost::variant:   9236 ms; S=7.14286e+14
  std::variant:   6279 ms; S=7.14286e+14
```

### -O3 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 2509 ms
boost::variant (-DONLY_BV): 3845 ms
  std::variant (-DONLY_SV): 2638 ms
```

#### Run time

```
N=100000000:
        double:   1592 ms; S=7.14286e+14
      variant2:   2697 ms; S=7.14286e+14
boost::variant:   6648 ms; S=7.14286e+14
  std::variant:   3826 ms; S=7.14286e+14

N=100000000:
        double:   1614 ms; S=7.14286e+14
      variant2:   8035 ms; S=7.14286e+14
boost::variant:   9221 ms; S=7.14286e+14
  std::variant:   6319 ms; S=7.14286e+14
```
