+++
title = "`void override_outcome_exception(basic_outcome<T, EC, EP, NoValuePolicy> *, U &&) noexcept`"
description = "Overrides the exception to something other than what was constructed."
+++

Overrides the exception to something other than what was constructed. You *almost certainly* never
want to use this function. A much better way of overriding the exception returned is to create
a custom no-value policy which lazily synthesises a custom exception object at the point of need.

The only reason that this function exists is because some people have very corner case needs
where a custom no-value policy can't be used, and where move-constructing a new `outcome` from
an old `outcome` with the exception state replaced isn't possible (e.g. when the types are
non-copyable and non-moveable).

Unless you are in a situation where no other viable alternative exists, do not use this function.

*Overridable*: Not overridable.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::hooks`

*Header*: `<boost/outcome/basic_outcome.hpp>`
