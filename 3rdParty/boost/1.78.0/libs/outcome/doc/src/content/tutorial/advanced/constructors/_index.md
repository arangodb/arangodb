+++
title = "Result returning constructors"
description = "How to metaprogram construction of objects which use result<T, EC> to return failure instead of throwing a C++ exception."
weight = 47
tags = ["constructors"]
+++

An oft-asked question during conference talks on Expected/Outcome is how to
exclusively use `result` to implement constructor failure. This is asked because
whilst almost every member function in a class can return a `result`, constructors
do not return values and thus cannot return a `result`. The implication is
that one cannot avoid throwing C++ exceptions to abort a construction.

As with most things in C++, one can achieve zero-exception-throw object
construction using a lot of
extra typing of boilerplate, and a little bit of simple C++ metaprogramming. This section
shows you how to implement these for those who are absolutely adverse to ever throwing an exception,
or cannot because C++ exceptions have been globally disabled.

The technique described here is not suitable for non-copyable and non-movable
types. There is also an assumption that moving your type is cheap.
