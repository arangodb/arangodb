+++
title = "Interoperation"
description = "Interoperating with std::expected<T, E> and other ValueOrError concept matching types."
weight = 80
+++

This is the final section of the tutorial, and it is unavoidably quite lengthy
as we are going to tie together all of the material covered in the tutorial
so far into a single, unified, application of Outcome's facilities.

One thing which Outcome solves -- and which alternatives do not -- is how to
**non-intrusively** tie together multiple third party libraries, each using
Outcome -- or some other `T|E` implementatation like {{% api "std::expected<T, E>" %}}
-- with custom incommensurate `E` types, or indeed arbitrary return
types which are "split" `T|E` return types. Solving
this well is the *coup de grâce* of Outcome against alternative approaches
to this problem domain,
including `std::expected<T, E>`. It is the major reason why you should
consider using Outcome over alternatives, including Expected.

Firstly we shall explore some of the problems faced by the software
developer when `T|E` return type based code proliferates at scale,
where dozens of libraries may be using completely incompatible `T|E` return types.

Secondly we shall introduce the `ValueOrError` concept support in Outcome,
which implements a subset of the proposed [WG21 `ValueOrError`
concept framework](https://wg21.link/P0786).

Finally, we shall then step through a worked example which mocks up a realistic
situation that the software developer may find themselves in: tying
together disparate third party libraries, whose source code cannot be
modified, into an application-wide, mixed-mode `T|E` and exception
throwing universal error handling system which is capable of
accurately representing the original failure, but also propagating it
in a way that the application can deal with universally.
