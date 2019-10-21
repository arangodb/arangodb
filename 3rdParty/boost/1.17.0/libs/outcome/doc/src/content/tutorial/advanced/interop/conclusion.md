+++
title = "Conclusion"
weight = 55
+++

This worked example was in fact excessively complex: a quicker route
to achieving the same thing would be to add explicit converting constructors
to `app::error_code` for each of the third party library `E` types.
One then could have saved oneself with having to bother injecting
custom converters into the `BOOST_OUTCOME_V2_NAMESPACE::convert` namespace.
If you control your application's `E` type, then that is probably a
better, and certainly simpler, approach.

However there are occasions when you don't have control over the
implementation of the destination `E` type e.g. in callbacks. Outcome's `ValueOrError`
infrastructure lets you inject custom interop code for any pair
of incommensurate third party `E` types, without needing to modify either's
source code.

This is without doubt a "power users" feature, but
one which will prove useful as `T|E` based C++ code proliferates.
