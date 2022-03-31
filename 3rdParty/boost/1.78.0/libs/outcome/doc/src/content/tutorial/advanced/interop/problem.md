+++
title = "Incommensurate E types"
weight = 5
+++

Back in the essential tutorial section on `result`, we studied a likely very common
initial choice of `E` type: [a strongly typed enum]({{< relref "/tutorial/essential/result" >}}).
We saw how [by marking up strongly typed enums to tell the C++ standard library
what they are]({{< relref "/motivation/plug_error_code" >}}), they gain implicit convertibility into `std::error_code`, and we
then pointed out that you might as well now always set `E = std::error_code`, as that
comes with the enormous advantage that you can use the boilerplate saving
`BOOST_OUTCOME_TRY` macro when the `E` type is always the same.

We thus strongly recommend to users that for any given piece of code, always
using the same `E` type across the codebase is very wise, except where you explicitly want
to prevent implicit propagation of failure up a call stack e.g. local failures in
some domain specific piece of code.

However it is unreasonable to expect that any non-trivial codebase can make do
with `E = std::error_code`. This is why Outcome allows you to use [custom `E`
types which carry payload](../../payload) in addition to an error code, yet
still have that custom type treated as if a `std::error_code`, including [lazy custom exception
throw synthesis](../../payload/copy_file3).

All this is good, but if library A uses `result<T, libraryA::failure_info>`,
and library B uses `result<T, libraryB::error_info>` and so on, there becomes
a problem for the application writer who is bringing in these third party
dependencies and tying them together into an application. As a general rule,
each third party library author will not have built in explicit interoperation
support for unknown other third party libraries. The problem therefore lands
with the application writer.

The application writer has one of three choices:

1. In the application, the form of result used is `result<T, std::variant<E1, E2, ...>>`
where `E1`, `E2` ... are the failure types for every third party library
in use in the application. This has the advantage of preserving the original
information exactly, but comes with a certain amount of use inconvenience
and maybe excessive coupling between high level layers and implementation detail.

2. One can translate/map the third party's failure type into the application's
failure type at the point of the failure
exiting the third party library and entering the application. One might do
this, say, with a C preprocessor macro wrapping every invocation of the third
party API from the application. This approach may lose the original failure detail,
or mis-map under certain circumstances if the mapping between the two systems
is not one-one.

3. One can type erase the third party's failure type into some application
failure type, which can later be reconstituted if necessary. This is the cleanest
solution with the least coupling issues and no problems with mis-mapping, but
it almost certainly requires the use of `malloc`, which the previous two did not.


Things get even more complicated in the presence of callbacks. If in the
callback you supply to library A, you call library B, you may need to insert
switch statement maps or other mechanisms to convert library B's failures into
something library A can understand, and then somehow extract that out -- preferably
without loss of original information -- into the application's failure handling
mechanism if library A subsequently returns failure as well. This implies
transmitting state by which to track these interrelated pieces of failure data.

Let us see what Outcome can do to help the application writer address some of these
issues, next.
