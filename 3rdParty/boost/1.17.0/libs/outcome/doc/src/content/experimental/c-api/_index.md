+++
title = "Using Outcome from C code"
description = "Interacting with `result` returning C++ functions from C code."
weight = 90
+++

A long standing problem for C code (or more usually nowadays, the many other programming
languages which can speak the C ABI but not the C++ ABI) is how to interpret C++ exception throws. The answer
is of course that they cannot, thus requiring one to write C shim code on the C++ side
of things of the form:

```c++
// The API we wish to expose to C
const char *get_value(double v);

// The C shim function for the C++ get_value() function.
extern "C" int c_get_value(const char **ret, double v)
{
  try
  {
    *ret = get_value(v);
    return 0;  // success
  }
  catch(const std::range_error &)
  {
    return ERANGE;
  }
  // More catch clauses may go in here ...
  catch(...)
  {
    return EAGAIN;
  }
}
```

This is sufficiently painful that most reach for a bindings generator tool like
[SWIG](http://www.swig.org/) to automate this sort of tedious boilerplate generation.
And this is fine for larger projects, but for smaller projects the cost of
setting up and configuring SWIG is also non-trivial.

What would be really great is if `result<T>` returning `noexcept` C++ functions
could be used straight from C. And indeed Experimental Outcome provides just that facility
which this section covers next.
