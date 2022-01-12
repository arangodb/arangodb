+++
title = "TRY avoiding copy/move"
description = ""
weight = 50
tags = ["try"]
+++

{{< api "BOOST_OUTCOME_TRYV(expr)/BOOST_OUTCOME_TRY(expr)" >}} works by creating an internal uniquely
named variable which holds the value emitted by the expression. This implies that a copy
or move operation shall be performed on the object emitted (unless you are on C++ 17 or
later, which has guaranteed copy elision), which may be undesirable for your use case.

You can tell `BOOST_OUTCOME_TRY` to use a reference rather than a value for the internal
uniquely named variable like this:

```c++
// This refers to a Result containing an immovable object
outcome::result<Immovable> &&res;

// For when you do want to extract the value
// This creates an auto &&unique = res, followed by an
// auto &&v = std::move(unique).assume_value()
BOOST_OUTCOME_TRY((auto &&, v), res);
```

If you don't care about extracting the value:

```c++
// For when you don't want to extract the value
// This creates an auto &&unique = res
BOOST_OUTCOME_TRYV2(auto &&, res);
```
