+++
title = "`std::ostream &operator<<(std::ostream &, const basic_result<T, E, NoValuePolicy> &)`"
description = "Serialises a `basic_result` to a `std::ostream`."
+++

Serialises a `basic_result` to a `std::ostream`.

Serialisation format is:

```
<unsigned int flags><space><value_type if set and not void><error_type if set and not void>
```

This is the **wrong** function to use if you wish to print human readable output.
Use {{% api "std::string print(const basic_result<T, E, NoValuePolicy> &)" %}} instead.

*Overridable*: Not overridable.

*Requires*: That `operator<<` is a valid expression for `std::ostream` and `T` and `E`.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/iostream_support.hpp>` (must be explicitly included manually).
