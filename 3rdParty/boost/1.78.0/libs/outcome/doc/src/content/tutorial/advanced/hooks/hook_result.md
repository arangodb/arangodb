+++
title = "Hook result"
description = ""
weight = 30
+++

We now tell Outcome that for every instance of our localised `result<T>`, that
on failure construction only, we want custom code to be run which increments the current
slot in TLS storage and writes the current stack backtrace into it.

For Outcome before v2.2, we must do this by inserting a specially named free function into
a namespace searched by ADL:

{{% snippet "error_code_extended.cpp" "error_code_extended3" %}}

For Outcome v2.2 and later, we must do this by using a custom no value policy which contains
a function named `on_result_construction()`. The function implementation is identical between
both mechanisms, just the name and placement of the function declaration differs.

The only non-obvious part above is the call to {{< api "void set_spare_storage(basic_result|basic_outcome *, uint16_t) noexcept" >}}.

Both `result` and `outcome` keep their internal state metadata in a `uint32_t`,
half of which is not used by Outcome. As it can be very useful to keep a small
unique number attached to any particular `result` or `outcome` instance, we
permit user code to set those sixteen bits to anything they feel like.
The corresponding function to retrieve those sixteen bits is {{< api "uint16_t spare_storage(const basic_result|basic_outcome *) noexcept" >}}.

The state of the sixteen bits of spare storage are ignored during comparison operations.

The sixteen bits of spare storage propagate during the following operations:

1. Copy and move construction between `result`'s.
2. Copy and move construction between `outcome`'s.
3. Copy and move construction from a `result` to an `outcome`.
4. Converting copy and move constructions for all the above.
5. Assignment for all of the above.

They are NOT propagated in these operations:

1. Any conversion or translation which goes through a `failure_type` or `success_type`.
2. Any conversion or translation which goes through a `ValueOrError` concept match.
3. Any unpacking or repacking of value/error/exception e.g. a manual repack of an
`outcome` into a `result`.
