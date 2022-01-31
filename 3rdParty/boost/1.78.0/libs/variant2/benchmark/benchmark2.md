# benchmark2.cpp results

## VS 2017 15.9.7 64 bit (cl.exe 19.16, /EHsc /std:c++17)

### /Od

#### Compile time

```
      variant2 (-DONLY_V2): 1403 ms
boost::variant (-DONLY_BV): 2972 ms
  std::variant (-DONLY_SV): 1057 ms
```

#### Run time

```
N=100000000:
        prefix:   7016 ms; S=416666583333336
      variant2:  24723 ms; S=416666583333336
boost::variant:  60438 ms; S=416666583333336
  std::variant:  20707 ms; S=416666583333336
```

### /O2 /DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 1778 ms
boost::variant (-DONLY_BV): 3252 ms
  std::variant (-DONLY_SV): 1372 ms
```

#### Run time

```
N=100000000:
        prefix:    803 ms; S=416666583333336
      variant2:   2124 ms; S=416666583333336
boost::variant:   6191 ms; S=416666583333336
  std::variant:   2193 ms; S=416666583333336
```

## g++ 7.4.0 -std=c++17 (Cygwin 64 bit)

### -O0

#### Compile time

```
      variant2 (-DONLY_V2): 1739 ms
boost::variant (-DONLY_BV): 3113 ms
  std::variant (-DONLY_SV): 1719 ms
```

#### Run time

```
N=100000000:
        prefix:   5163 ms; S=416666583333336
      variant2:  20628 ms; S=416666583333336
boost::variant:  43308 ms; S=416666583333336
  std::variant:  42375 ms; S=416666583333336
```

### -O1

#### Compile time

```
      variant2 (-DONLY_V2): 1484 ms
boost::variant (-DONLY_BV): 2947 ms
  std::variant (-DONLY_SV): 1448 ms
```

#### Run time

```
N=100000000:
        prefix:    781 ms; S=416666583333336
      variant2:   1992 ms; S=416666583333336
boost::variant:   2249 ms; S=416666583333336
  std::variant:   4843 ms; S=416666583333336
```

### -O2 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 1547 ms
boost::variant (-DONLY_BV): 2999 ms
  std::variant (-DONLY_SV): 1528 ms
```

#### Run time

```
N=100000000:
        prefix:    793 ms; S=416666583333336
      variant2:   1686 ms; S=416666583333336
boost::variant:   1833 ms; S=416666583333336
  std::variant:   4340 ms; S=416666583333336
```

### -O3 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 1595 ms
boost::variant (-DONLY_BV): 3084 ms
  std::variant (-DONLY_SV): 1620 ms
```

#### Run time

```
N=100000000:
        prefix:    853 ms; S=416666583333336
      variant2:   1681 ms; S=416666583333336
boost::variant:   1773 ms; S=416666583333336
  std::variant:   3989 ms; S=416666583333336
```

## clang++ 5.0.1 -std=c++17 -stdlib=libc++ (Cygwin 64 bit)

### -O0

#### Compile time

```
      variant2 (-DONLY_V2): 1578 ms
boost::variant (-DONLY_BV): 2623 ms
  std::variant (-DONLY_SV): 1508 ms
```

#### Run time

```
N=100000000:
        prefix:   4447 ms; S=416666583333336
      variant2:  16016 ms; S=416666583333336
boost::variant:  42365 ms; S=416666583333336
  std::variant:  17817 ms; S=416666583333336
```

### -O1

#### Compile time

```
      variant2 (-DONLY_V2): 1841 ms
boost::variant (-DONLY_BV): 2919 ms
  std::variant (-DONLY_SV): 1776 ms
```

#### Run time

```
N=100000000:
        prefix:   1390 ms; S=416666583333336
      variant2:   5397 ms; S=416666583333336
boost::variant:  23234 ms; S=416666583333336
  std::variant:   2807 ms; S=416666583333336
```

### -O2 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 1766 ms
boost::variant (-DONLY_BV): 2817 ms
  std::variant (-DONLY_SV): 1718 ms
```

#### Run time

```
N=100000000:
        prefix:    604 ms; S=416666583333336
      variant2:   1625 ms; S=416666583333336
boost::variant:   2735 ms; S=416666583333336
  std::variant:   2664 ms; S=416666583333336
```

### -O3 -DNDEBUG

#### Compile time

```
      variant2 (-DONLY_V2): 1720 ms
boost::variant (-DONLY_BV): 2806 ms
  std::variant (-DONLY_SV): 1737 ms
```

#### Run time

```
N=100000000:
        prefix:    603 ms; S=416666583333336
      variant2:   1608 ms; S=416666583333336
boost::variant:   2696 ms; S=416666583333336
  std::variant:   2668 ms; S=416666583333336
```
