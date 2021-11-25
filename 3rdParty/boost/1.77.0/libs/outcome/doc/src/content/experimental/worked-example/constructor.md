+++
title = "The constructor"
weight = 30
+++

Code domains are 100% constexpr to construct and destruct, as are status codes.
This enables the compiler to 100% instantiate both only in its mind, and to emit
zero code and thus zero overhead.

Unfortunately it also means that it must be possible for each domain to be instantiated an infinite
number of times, and being 100% in constexpr, any instances never have a unique
address in memory either. Thus we cannot compare domains for equivalence using
their address in memory, as {{% api "std::error_category" %}} does.

We solve this by using a *very* random 64 bit number taken from a hard random
number source. The website https://www.random.org/cgi-bin/randbyte?nbytes=8&format=h
is strongly suggested as the source for this number.

(In case you are wondering about the chance of collision for a 64 bit integer, SG14 estimated that
approximately 190,000 separate domains would need to exist in a single process
for there to be a 0.00000001% probability of collision if the random number
source is very random)

{{% snippet "experimental_status_code.cpp" "constructor" %}}

A nice side effect of this approach is that custom error domains in header-only
libraries are safe, unlike custom `<system_error>` error categories. Boost.System's
error categories can now opt into this same safe mechanism in order to also be
safe in header only library use cases.
