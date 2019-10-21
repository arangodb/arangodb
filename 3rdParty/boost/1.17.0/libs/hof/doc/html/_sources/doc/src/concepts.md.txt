Concepts
========

ConstFunctionObject
-------------------

Is an object with a `const` call operator:

```cpp
concept ConstFunctionObject
{
    template<class... Ts>
    auto operator()(Ts&&...) const;
};
```

#### Requirements:

The type `F` satisfies `ConstFunctionObject` if

* The type `F` satisfies `std::is_object`, and 

Given

* `f`, an object of type `const F`
* `args...`, suitable argument list, which may be empty 

```eval_rst
+----------------+--------------------------+
| Expression     | Requirements             |
+================+==========================+
| ``f(args...)`` | performs a function call |
+----------------+--------------------------+
```

NullaryFunctionObject
---------------------

Is an object with a `const` call operator that accepts no parameters:

```cpp
concept NullaryFunctionObject
{
    auto operator()() const;
};
```

#### Requirements:

* `ConstFunctionObject`

Given

* `f`, an object of type `const F`

```eval_rst
+----------------+--------------------------+
| Expression     | Requirements             |
+================+==========================+
| ``f()``        | performs a function call |
+----------------+--------------------------+
```

UnaryFunctionObject
-------------------

Is an object with a `const` call operator that accepts one parameter:

```cpp
concept UnaryFunctionObject
{
    template<class T>
    auto operator()(T&&) const;
};
```

#### Requirements:

* `ConstFunctionObject`

Given

* `f`, an object of type `const F`
* `arg`, a single argument

```eval_rst
+----------------+--------------------------+
| Expression     | Requirements             |
+================+==========================+
| ``f(arg)``     | performs a function call |
+----------------+--------------------------+
```

BinaryFunctionObject
--------------------

Is an object with a `const` call operator that accepts two parameter:

```cpp
concept UnaryFunctionObject
{
    template<class T, class U>
    auto operator()(T&&, U&&) const;
};
```

#### Requirements:

* `ConstFunctionObject`

Given

* `f`, an object of type `const F`
* `arg1`, a single argument
* `arg2`, a single argument

```eval_rst
+-------------------+--------------------------+
| Expression        | Requirements             |
+===================+==========================+
| ``f(arg1, arg2)`` | performs a function call |
+-------------------+--------------------------+
```

MutableFunctionObject
---------------------

Is an object with a `mutable` call operator:

```cpp
concept MutableFunctionObject
{
    template<class... Ts>
    auto operator()(Ts&&...);
};
```

#### Requirements:

The type `F` satisfies `MutableFunctionObject` if

* The type `F` satisfies `std::is_object`, and 

Given

* `f`, an object of type `F`
* `args...`, suitable argument list, which may be empty 

```eval_rst
+----------------+--------------------------+
| Expression     | Requirements             |
+================+==========================+
| ``f(args...)`` | performs a function call |
+----------------+--------------------------+
```

EvaluatableFunctionObject
-------------------------

Is an object that is either a `NullaryFunctionObject`, or it is an `UnaryFuntionObject` that accepts the `identity` function as a parameter.

#### Requirements:

* `NullaryFunctionObject`

Given

* `f`, an object of type `const F`

```eval_rst
+----------------+--------------------------+
| Expression     | Requirements             |
+================+==========================+
| ``f()``        | performs a function call |
+----------------+--------------------------+
```

Or:

* `UnaryFuntionObject`

Given

* `f`, an object of type `const F`
* `identity`, which is the identity function

```eval_rst
+-----------------+--------------------------+
| Expression      | Requirements             |
+=================+==========================+
| ``f(identity)`` | performs a function call |
+-----------------+--------------------------+
```

Invocable
---------

Is an object for which the `INVOKE` operation can be applied.

#### Requirements:

The type `T` satisfies `Invocable` if

Given

* `f`, an object of type `T`
* `Args...`, suitable list of argument types

The following expressions must be valid: 

```eval_rst
+----------------------------------------+-------------------------------------------------------+
| Expression                             | Requirements                                          |
+========================================+=======================================================+
| ``INVOKE(f, std::declval<Args>()...)`` | the expression is well-formed in unevaluated context  |
+----------------------------------------+-------------------------------------------------------+
```

where `INVOKE(f, x, xs...)` is defined as follows:

* if `f` is a pointer to member function of class `U`: 

    - If `std::is_base_of<U, std::decay_t<decltype(x)>>()` is true, then `INVOKE(f, x, xs...)` is equivalent to `(x.*f)(xs...)`
    - otherwise, if `std::decay_t<decltype(x)>` is a specialization of `std::reference_wrapper`, then `INVOKE(f, x, xs...)` is equivalent to `(x.get().*f)(xs...)` 
    - otherwise, if x does not satisfy the previous items, then `INVOKE(f, x, xs...)` is equivalent to `((*x).*f)(xs...)`. 

* otherwise, if `f` is a pointer to data member of class `U`: 

    - If `std::is_base_of<U, std::decay_t<decltype(x)>>()` is true, then `INVOKE(f, x)` is equivalent to `x.*f`
    - otherwise, if `std::decay_t<decltype(x)>` is a specialization of `std::reference_wrapper`, then `INVOKE(f, x)` is equivalent to `x.get().*f`
    - otherwise, if `x` does not satisfy the previous items, then `INVOKE(f, x)` is equivalent to `(*x).*f`

* otherwise, `INVOKE(f, x, xs...)` is equivalent to `f(x, xs...)`

ConstInvocable
-------------

Is an object for which the `INVOKE` operation can be applied.

#### Requirements:

The type `T` satisfies `ConstInvocable` if

Given

* `f`, an object of type `const T`
* `Args...`, suitable list of argument types

The following expressions must be valid: 

```eval_rst
+----------------------------------------+-------------------------------------------------------+
| Expression                             | Requirements                                          |
+========================================+=======================================================+
| ``INVOKE(f, std::declval<Args>()...)`` | the expression is well-formed in unevaluated context  |
+----------------------------------------+-------------------------------------------------------+
```

where `INVOKE(f, x, xs...)` is defined as follows:

* if `f` is a pointer to member function of class `U`: 

    - If `std::is_base_of<U, std::decay_t<decltype(x)>>()` is true, then `INVOKE(f, x, xs...)` is equivalent to `(x.*f)(xs...)`
    - otherwise, if `std::decay_t<decltype(x)>` is a specialization of `std::reference_wrapper`, then `INVOKE(f, x, xs...)` is equivalent to `(x.get().*f)(xs...)` 
    - otherwise, if x does not satisfy the previous items, then `INVOKE(f, x, xs...)` is equivalent to `((*x).*f)(xs...)`. 

* otherwise, if `f` is a pointer to data member of class `U`: 

    - If `std::is_base_of<U, std::decay_t<decltype(x)>>()` is true, then `INVOKE(f, x)` is equivalent to `x.*f`
    - otherwise, if `std::decay_t<decltype(x)>` is a specialization of `std::reference_wrapper`, then `INVOKE(f, x)` is equivalent to `x.get().*f`
    - otherwise, if `x` does not satisfy the previous items, then `INVOKE(f, x)` is equivalent to `(*x).*f`

* otherwise, `INVOKE(f, x, xs...)` is equivalent to `f(x, xs...)`

UnaryInvocable
--------------

Is an object for which the `INVOKE` operation can be applied with one parameter.

#### Requirements:

* `ConstInvocable`

Given

* `f`, an object of type `const F`
* `arg`, a single argument

```eval_rst
+----------------------------------------+-------------------------------------------------------+
| Expression                             | Requirements                                          |
+========================================+=======================================================+
| ``INVOKE(f, arg)``                     | the expression is well-formed in unevaluated context  |
+----------------------------------------+-------------------------------------------------------+
```

BinaryInvocable
---------------

Is an object for which the `INVOKE` operation can be applied with two parameters.

#### Requirements:

* `ConstInvocable`

Given

* `f`, an object of type `const F`
* `arg1`, a single argument
* `arg2`, a single argument

```eval_rst
+----------------------------------------+-------------------------------------------------------+
| Expression                             | Requirements                                          |
+========================================+=======================================================+
| ``INVOKE(f, arg1, arg2)``              | the expression is well-formed in unevaluated context  |
+----------------------------------------+-------------------------------------------------------+
```

Metafunction
------------

Given

* `f`, a type or a template
* `args...`, any suitable type, which may be empty

```eval_rst
+--------------------------+--------------------------------------------+
| Expression               | Requirements                               |
+==========================+============================================+
| ``f::type``              | The type is the result of the metafunction |
+--------------------------+--------------------------------------------+
| ``f<args...>::type``     | The type is the result of the metafunction |
+--------------------------+--------------------------------------------+
```

MetafunctionClass
-----------------

Given

* `f`, a type or a template
* `args...`, any suitable type, which may be empty

```eval_rst
+-----------------------------+--------------------------------------------+
| Expression                  | Requirements                               |
+=============================+============================================+
| ``f::apply::type``          | The type is the result of the metafunction |
+-----------------------------+--------------------------------------------+
| ``f::apply<args...>::type`` | The type is the result of the metafunction |
+-----------------------------+--------------------------------------------+
```
