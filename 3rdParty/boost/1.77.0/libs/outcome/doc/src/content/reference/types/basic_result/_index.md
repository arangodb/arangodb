+++
title = "`basic_result<T, E, NoValuePolicy>`"
description = "A sum type carrying either a successful `T`, or a disappointment `E`, with `NoValuePolicy` specifying what to do if one tries to read state which isn't there."
+++

A sum type carrying either a `T` or an `E`, with `NoValuePolicy` specifying what to do if one tries to read state which isn't there, and enabling injection of hooks to trap when lifecycle events occur. Either or both of `T` and `E` can be `void` to indicate no value for that state is present. Note that `E = void` makes basic result into effectively an `optional<T>`, but with `NoValuePolicy` configurable behaviour. Detectable using {{% api "is_basic_result<T>" %}}.

*Requires*: Concept requirements if C++ 20, else static asserted:

- That trait {{% api "type_can_be_used_in_basic_result<R>" %}} is true for both `T` and `E`.
- That either `E` is `void` or `DefaultConstructible` (Outcome v2.1 and earlier only).

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/basic_result.hpp>`

*Inclusions*: The very lightest weight of C and C++ header files:

1. `<cstdint>`
2. `<initializer_list>`
3. `<iosfwd>`
4. `<new>`
5. `<type_traits>`
6. If {{% api "BOOST_OUTCOME_USE_STD_IN_PLACE_TYPE" %}} is `1`, `<utility>` (defaults to `1` for C++ 17 or later only)
7. If C++ exceptions disabled and `BOOST_OUTCOME_DISABLE_EXECINFO` undefined only (used to print stack backtraces on "exception throw"):
    1. `<sal.h>` (Windows only)
    2. `<stddef.h>` (Windows only)
    3. `<string.h>` (Windows only)
    4. `<execinfo.h>` (POSIX only)
8. `<cstdio>`
9. `<cstdlib>`
10. `<cassert>`

This very light weight set of inclusion dependencies makes basic result suitable for use in global header files of very large C++ codebases.

### Design rationale

The basic result type is the main workhorse type of the Outcome library, providing a simple sum type with optional values representing success or disappointment. Unlike {{% api "std::expected<T, E>" %}}, Outcome's result type is designed specifically for convenience when implementing failure handling across very large codebases, and it has a number of API differences to facilitate that.

The first major design difference is that basic result models its constructor design on {{% api "std::variant<...>" %}}, rather than modelling {{% api "std::optional<T>" %}}'s constructor design like `std::expected<T, E>` does. This means that basic result will implicitly construct either a `T` or an `E` if doing so is unambiguous, same as `variant` does. Where implicit construction is ambiguous, the implicit constructors disable and a `T` or `E` can be specified via {{% api "in_place_type_t<T>" %}}, or via {{% api "success_type<T>" %}} or {{% api "failure_type<T>" %}}. We implement a subset of variant's constructors for improved compile time impact, so the implicit and explicit constructor design is split into fixed subsets to reduce SFINAE execution.

The second major design difference is that union storage is ONLY used when both `T` and `E` are trivially copyable or `void`, otherwise struct storage is used. This is usually not a problem, as it is assumed that `sizeof(E)` will be small for failure handling. The choice to only use union storage for trivially copyable types only very considerably reduces load on the compiler, and substantially improves compile times in very large C++ 14 codebases, because copies and moves do not need to jump through complex ceremony in order to implement the never-empty guarantees which would be required in a union storage based implementation (C++ 17 onwards does far fewer copy and move constructor instantiations, but it all adds up -- work avoided is always the fastest).

### Public member type aliases

- `value_type` is `T`.
- `error_type` is `E`.
- `no_value_policy_type` is `NoValuePolicy`.
- `value_type_if_enabled` is `T` if construction from `T` is available, else it is a usefully named unusable internal type.
- `error_type_if_enabled` is `E` if construction from `E` is available, else it is a usefully named unusable internal type.
- `rebind<A, B = E, C = NoValuePolicy>` is `basic_result<A, B, C>`.

### Protected member predicate booleans

- `predicate::constructors_enabled` is constexpr boolean true if decayed `value_type` and decayed `error_type` are not the same type.

- `predicate::implicit_constructors_enabled` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Trait {{% api "is_error_type<E>" %}} is not true for both decayed `value_type` and decayed `error_type` at the same time.
    3. `value_type` is not implicitly constructible from `error_type` and `error_type` is not implicitly constructible from `value_type`.<br>OR<br>trait {{% api "is_error_type<E>" %}} is true for decayed `error_type` and `error_type` is not implicitly constructible from `value_type` and `value_type` is an integral type.

- `predicate::enable_value_converting_constructor<A>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `A` is not this `basic_result` type.
    3. `predicate::implicit_constructors_enabled` is true.
    4. Decayed `A` is not an `in_place_type_t`.
    5. Trait {{% api "is_error_type_enum<E, Enum>" %}} is false for `error_type` and decayed `A`.
    6. `value_type` is implicitly constructible from `A` and `error_type` is not implicitly constructible from `A`.<br>OR<br>`value_type` is the exact same type as decayed `A` and `value_type` is implicitly constructible from `A`.

- `predicate::enable_error_converting_constructor<A>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `A` is not this `basic_result` type.
    3. `predicate::implicit_constructors_enabled` is true.
    4. Decayed `A` is not an `in_place_type_t`.
    5. Trait {{% api "is_error_type_enum<E, Enum>" %}} is false for `error_type` and decayed `A`.
    6. `value_type` is not implicitly constructible from `A` and `error_type` is implicitly constructible from `A`.<br>OR<br>`error_type` is the exact same type as decayed `A` and `error_type` is implicitly constructible from `A`.

- `predicate::enable_error_condition_converting_constructor<ErrorCondEnum>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `ErrorCondEnum` is not this `basic_result` type.
    3. Decayed `ErrorCondEnum` is not an `in_place_type_t`.
    4. Trait {{% api "is_error_type_enum<E, Enum>" %}} is true for `error_type` and decayed `ErrorCondEnum`.

- `predicate::enable_compatible_conversion<A, B, C>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `basic_result<A, B, C>` is not this `basic_result` type.
    3. `A` is `void` OR `value_type` is explicitly constructible from `A`.
    4. `B` is `void` OR `error_type` is explicitly constructible from `B`.

- `predicate::enable_make_error_code_compatible_conversion<A, B, C>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `basic_result<A, B, C>` is not this `basic_result` type.
    3. Trait {{% api "is_error_code_available<T>" %}} is true for decayed `error_type`.
    4. `predicate::enable_compatible_conversion<A, B, C>` is not true.
    5. `A` is `void` OR `value_type` is explicitly constructible from `A`.
    6. `error_type` is explicitly constructible from `make_error_code(B)`.

- `predicate::enable_make_exception_ptr_compatible_conversion<A, B, C>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `basic_result<A, B, C>` is not this `basic_result` type.
    3. Trait {{% api "is_exception_ptr_available<T>" %}} is true for decayed `error_type`.
    4. `predicate::enable_compatible_conversion<A, B, C>` is not true.
    5. `A` is `void` OR `value_type` is explicitly constructible from `A`.
    6. `error_type` is explicitly constructible from `make_exception_ptr(B)`.

- `predicate::enable_inplace_value_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `value_type` is `void` OR `value_type` is explicitly constructible from `Args...`.

- `predicate::enable_inplace_error_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `error_type` is `void` OR `error_type` is explicitly constructible from `Args...`.

- `predicate::enable_inplace_value_error_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `predicate::implicit_constructors_enabled` is true.
    3. Either, but not both, of `value_type` is explicitly constructible from `Args...` or `error_type` is explicitly constructible from `Args...`.

#### Summary of [standard requirements provided](https://en.cppreference.com/w/cpp/named_req)

- ~~`DefaultConstructible`~~, always deleted to force user to choose valued or errored for every result instanced.
- `MoveConstructible`, if both `value_type` and `error_type` implement move constructors.
- `CopyConstructible`, if both `value_type` and `error_type` implement copy constructors.
- `MoveAssignable`, if both `value_type` and `error_type` implement move constructors and move assignment.
- `CopyAssignable`, if both `value_type` and `error_type` implement copy constructors and copy assignment.
- `Destructible`.
- `Swappable`, with the strong rather than weak guarantee. See {{% api "void swap(basic_result &)" %}} for more information.
- `TriviallyCopyable`, if both `value_type` and `error_type` are trivially copyable.
- `TrivialType`, if both `value_type` and `error_type` are trivial types.
- `LiteralType`, if both `value_type` and `error_type` are literal types.
- `StandardLayoutType`, if both `value_type` and `error_type` are standard layout types. If so, layout of `basic_result` in C is guaranteed to be one of:

    ```c++
    struct trivially_copyable_result_layout {
      union {
        value_type value;
        error_type error;
      };
      unsigned int flags;
    };
    ```

    ... if both `value_type` and `error_type` are `TriviallyCopyable`, otherwise:

    ```c++
    struct non_trivially_copyable_result_layout {
      value_type value;
      unsigned int flags;
      error_type error;
    };
    ```
    
    Obviously, all C compatible C++ types are `TriviallyCopyable`, so if you are passing non-trivially copyable types from C++ to C, you are doing undefined behaviour.
- `EqualityComparable`, if both `value_type` and `error_type` implement equality comparisons with one another.
- ~~`LessThanComparable`~~, not implemented due to availability of implicit conversions from `value_type` and `error_type`, this can cause major surprise (i.e. hard to diagnose bugs), so we don't implement these at all.
- ~~`Hash`~~, not implemented as a generic implementation of a unique hash for non-valued items which are unequal would require a dependency on RTTI being enabled.

Thus `basic_result` meets the `Regular` concept if both `value_type` and `error_type` are `Regular`, except for the lack of a default constructor. Often where one needs a default constructor, wrapping `basic_result` into {{% api "std::optional<T>" %}} will suffice.

### Public member functions

#### Disabling constructors

{{% children description="true" depth="2" categories="disabling-constructors" %}}

#### Copy and move constructors and assignment, and destructor

{{% children description="true" depth="2" categories="default-constructors,copy-constructors,move-constructors,copy-assignment,move-assignment,destructors" %}}

#### Converting constructors

{{% children description="true" depth="2" categories="converting-constructors" %}}

#### Inplace constructors

{{% children description="true" depth="2" categories="inplace-constructors" %}}

#### Tagged constructors

{{% children description="true" depth="2" categories="tagged-constructors" %}}

#### Observers

{{% children description="true" depth="2" categories="observers" %}}

#### Modifiers

{{% children description="true" depth="2" categories="modifiers" %}}

#### Comparisons

See above for why `LessThanComparable` is not implemented.

{{% children description="true" depth="2" categories="comparisons" %}}

