+++
title = "`std::istream &operator>>(std::istream &, basic_outcome<T, EC, EP, NoValuePolicy> &)`"
description = "Deserialises a `basic_outcome` from a `std::istream`."
+++

Deserialises a `basic_outcome` from a `std::istream`.

Serialisation format is:

```
<unsigned int flags><space><value_type if set and not void><error_type if set and not void><exception_type if set and not void>
```

*Overridable*: Not overridable.

*Requires*: That `operator>>` is a valid expression for `std::istream` and `T`, `EC` and `EP`.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/iostream_support.hpp>` (must be explicitly included manually).
