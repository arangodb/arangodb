<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Definitions
===========

Function Adaptor
----------------

A [function adaptor](<Function Adaptor>) takes a function(or functions) and returns a new function with enhanced capability. Each adaptor has a functional form with a corresponding class with `_adaptor` appended to it:

```cpp
template<class... Fs>
FunctionAdaptor_adaptor<Fs...> FunctionAdaptor(Fs...);
```

Both the functional form and the class form can be used to construct the adaptor.

Static Function Adaptor
-----------------------

A static function adaptor is a [function adaptor](<Function Adaptor>) that doesn't have a functional form. It is only a class. It has an additional requirement that the function is [`DefaultConstructible`](DefaultConstructible):

```cpp
template<class... Fs>
class StaticFunctionAdaptor;
```

Decorator
---------

A decorator is a function that returns a [function adaptor](<Function Adaptor>). The [function adaptor](<Function Adaptor>) may be an unspecified or private type.

```cpp
template<class... Ts>
FunctionAdaptor Decorator(Ts...);
```

Semantics
---------

Some parts of the documentation provides the meaning(or equivalence) of an expression. Here is a guide of those symbols:

* `f`, `g`, `fs`, `gs`, `p` are functions
* `x`, `y`, `xs`, `ys` are parameters to a function
* `T` represents some type
* `...` are parameter packs and represent varidiac parameters

Signatures
----------

All the functions are global function objects except where an explicit template parameter is required on older compilers. However, the documentation still shows the traditional signature since it is much clearer. So instead of writing this:

```cpp
struct if_f
{
    template<class IntegralConstant>
    constexpr auto operator()(IntegralConstant) const;
};
const constexpr if_f if_ = {};
```

The direct function signature is written even though it is actually declared like above:

```cpp
template<class IntegralConstant>
constexpr auto if_(IntegralConstant);
```

Its usage is the same except it has the extra benefit that the function can be directly passed to another function.

