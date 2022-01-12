+++
title = "`basic_outcome<T, EC, EP, NoValuePolicy>`"
description = "A type carrying one of (i) a successful `T` (ii) a disappointment `EC` (iii) a failure `EP` (iv) both a disappointment `EC` and a failure `EP`, with `NoValuePolicy` specifying what to do if one tries to read state which isn't there."
+++

A type carrying one of (i) a successful `T` (ii) a disappointment `EC` (iii) a failure `EP` (iv) both a disappointment `EC` and a failure `EP`, with `NoValuePolicy` specifying what to do if one tries to read state which isn't there, and enabling injection of hooks to trap when lifecycle events occur. Any one, two, or all of `T`, `EC` and `EP` can be `void` to indicate no value for that state is present. Detectable using {{% api "is_basic_outcome<T>" %}}.

*Requires*: Concept requirements if C++ 20, else static asserted:

- That trait {{% api "type_can_be_used_in_basic_result<R>" %}} is true for `T`, `EC` and `EP`.
- That either `EC` is `void` or `DefaultConstructible` (Outcome v2.1 and earlier only).
- That either `EP` is `void` or `DefaultConstructible`.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/basic_outcome.hpp>`

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

This very light weight set of inclusion dependencies makes basic outcome suitable for use in global header files of very large C++ codebases.

### Design rationale

`basic_outcome` extends {{% api "basic_result<T, E, NoValuePolicy>" %}} with a third state to transport,
conventionally (but not necessarily) some sort of "abort" or "exceptional" state which a function can
return to indicate that not only did the operation fail, but it did so *catastrophically* i.e. please
abort any attempt to retry the operation.

A perfect alternative is to throw a C++ exception for the abort code path, and indeed most programs
ought to do exactly that instead of using `basic_outcome`. However there are a number of use cases
where choosing `basic_outcome` shines:

1. Where C++ exceptions or RTTI is not available, but the ability to fail catastrophically without
terminating the program is important.
2. Where deterministic behaviour is required even in the catastrophic failure situation.
3. In unit test suites of code using Outcome it is extremely convenient to accumulate test failures
into a `basic_outcome` for later reporting. A similar convenience applies to RPC situations, where
C++ exception throws need to be accumulated for reporting back to the initiating endpoint.
4. Where a function is "dual use deterministic" i.e. it can be used deterministically, in which case
one switches control flow based on `.error()`, or it can be used non-deterministically by throwing
an exception perhaps carrying a custom payload.

### Public member type aliases

- `value_type` is `T`.
- `error_type` is `EC`.
- `exception_type` is `EP`.
- `no_value_policy_type` is `NoValuePolicy`.
- `value_type_if_enabled` is `T` if construction from `T` is available, else it is a usefully named unusable internal type.
- `error_type_if_enabled` is `EC` if construction from `EC` is available, else it is a usefully named unusable internal type.
- `exception_type_if_enabled` is `EP` if construction from `EP` is available, else it is a usefully named unusable internal type.
- `rebind<A, B = EC, C = EP, D = NoValuePolicy>` is `basic_outcome<A, B, C, D>`.

### Protected member predicate booleans

- `predicate::constructors_enabled` is constexpr boolean true if:
    1. Decayed `value_type` and decayed `error_type` are not the same type, or both are `void`.
    2. Decayed `value_type` and decayed `exception_type` are not the same type, or both are `void`.
    3. Decayed `error_type` and decayed `exception_type` are not the same type, or both are `void`.

- `predicate::implicit_constructors_enabled` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Trait {{% api "is_error_type<E>" %}} is not true for both decayed `value_type` and decayed `error_type` at the same time.
    3. `value_type` is not implicitly constructible from `error_type` and `error_type` is not implicitly constructible from `value_type`.<br>OR<br>trait {{% api "is_error_type<E>" %}} is true for decayed `error_type` and `error_type` is not implicitly constructible from `value_type` and `value_type` is an integral type.
    4. `value_type` is not implicitly constructible from `exception_type`.
    5. `error_type` is not implicitly constructible from `exception_type`.
    6. `exception_type` is not implicitly constructible from `value_type`.
    7. `exception_type` is not implicitly constructible from `error_type`.

- `predicate::enable_value_converting_constructor<A>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `A` is not this `basic_outcome` type.
    3. `predicate::implicit_constructors_enabled` is true.
    4. Decayed `A` is not an `in_place_type_t`.
    5. Trait {{% api "is_error_type_enum<E, Enum>" %}} is false for `error_type` and decayed `A`.
    6. `value_type` is implicitly constructible from `A` and `error_type` is not implicitly constructible from `A`.<br>OR<br>`value_type` is the exact same type as decayed `A` and `value_type` is implicitly constructible from `A`.
    7. `exception_type` is not implicitly constructible from `A`.

- `predicate::enable_error_converting_constructor<A>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `A` is not this `basic_outcome` type.
    3. `predicate::implicit_constructors_enabled` is true.
    4. Decayed `A` is not an `in_place_type_t`.
    5. Trait {{% api "is_error_type_enum<E, Enum>" %}} is false for `error_type` and decayed `A`.
    6. `value_type` is not implicitly constructible from `A` and `error_type` is implicitly constructible from `A`.<br>OR<br>`error_type` is the exact same type as decayed `A` and `error_type` is implicitly constructible from `A`.
    7. `exception_type` is not implicitly constructible from `A`.

- `predicate::enable_error_condition_converting_constructor<ErrorCondEnum>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `ErrorCondEnum` is not this `basic_outcome` type.
    3. Decayed `ErrorCondEnum` is not an `in_place_type_t`.
    4. Trait {{% api "is_error_type_enum<E, Enum>" %}} is true for `error_type` and decayed `ErrorCondEnum`.
    5. `exception_type` is not implicitly constructible from `ErrorCondEnum`.

- `predicate::enable_exception_converting_constructor<A>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `A` is not this `basic_outcome` type.
    3. `predicate::implicit_constructors_enabled` is true.
    4. Decayed `A` is not an `in_place_type_t`.
    5. `value_type` is not implicitly constructible from `A`.
    6. `error_type` is not implicitly constructible from `A`.
    7. `exception_type` is implicitly constructible from `A`.

- `predicate::enable_error_exception_converting_constructor<A, B>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. Decayed `A` is not this `basic_outcome` type.
    3. `predicate::implicit_constructors_enabled` is true.
    4. Decayed `A` is not an `in_place_type_t`.
    5. `value_type` is not implicitly constructible from `A`.
    6. `error_type` is implicitly constructible from `A`.
    7. `value_type` is not implicitly constructible from `B`.
    8. `exception_type` is implicitly constructible from `B`.

- `predicate::enable_compatible_conversion<A, B, C, D>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `basic_outcome<A, B, C, D>` is not this `basic_outcome` type.
    3. `A` is `void` OR `value_type` is explicitly constructible from `A`.
    4. `B` is `void` OR `error_type` is explicitly constructible from `B`.
    5. `C` is `void` OR `exception_type` is explicitly constructible from `C`.

- `predicate::enable_make_error_code_compatible_conversion<A, B, C, D>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `basic_outcome<A, B, C, D>` is not this `basic_outcome` type.
    3. Trait {{% api "is_error_code_available<T>" %}} is true for decayed `error_type`.
    4. `predicate::enable_compatible_conversion<A, B, C, D>` is not true.
    5. `A` is `void` OR `value_type` is explicitly constructible from `A`.
    6. `error_type` is explicitly constructible from `make_error_code(B)`.
    7. `C` is `void` OR `exception_type` is explicitly constructible from `C`.

- `predicate::enable_inplace_value_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `value_type` is `void` OR `value_type` is explicitly constructible from `Args...`.

- `predicate::enable_inplace_error_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `error_type` is `void` OR `error_type` is explicitly constructible from `Args...`.

- `predicate::enable_inplace_exception_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `exception_type` is `void` OR `exception_type` is explicitly constructible from `Args...`.

- `predicate::enable_inplace_value_error_exception_constructor<Args...>` is constexpr boolean true if:
    1. `predicate::constructors_enabled` is true.
    2. `predicate::implicit_constructors_enabled` is true.
    3. Exactly one of `value_type` is explicitly constructible from `Args...`, or `error_type` is explicitly constructible from `Args...`, or `exception_type` is explicitly constructible
    from `Args...`.

#### Summary of [standard requirements provided](https://en.cppreference.com/w/cpp/named_req)

- ~~`DefaultConstructible`~~, always deleted to force user to choose valued or errored or excepted for every outcome instanced.
- `MoveConstructible`, if all of `value_type`, `error_type` and `exception_type` implement move constructors.
- `CopyConstructible`, if all of `value_type`, `error_type` and `exception_type` implement copy constructors.
- `MoveAssignable`, if all of `value_type`, `error_type` and `exception_type` implement move constructors and move assignment.
- `CopyAssignable`, if all of `value_type`, `error_type` and `exception_type` implement copy constructors and copy assignment.
- `Destructible`.
- `Swappable`, with the strong rather than weak guarantee. See {{% api "void swap(basic_outcome &)" %}} for more information.
- `TriviallyCopyable`, if all of `value_type`, `error_type` and `exception_type` are trivially copyable.
- `TrivialType`, if all of `value_type`, `error_type` and `exception_type` are trivial types.
- `LiteralType`, if all of `value_type`, `error_type` and `exception_type` are literal types.
- ~~`StandardLayoutType`~~. It is implementation defined if `basic_outcome` can be used by C.
However all of the three major compilers MSVC, GCC and clang implement C layout of `basic_outcome` as follows:

    ```c++
    struct outcome_layout {
      struct trivially_copyable_result_layout {
        union {
          value_type value;
          error_type error;
        };
        unsigned int flags;
      };
      exception_type exception;
    };
    ```

    ... if both `value_type` and `error_type` are `TriviallyCopyable`, otherwise:

    ```c++
    struct outcome_layout {
      struct non_trivially_copyable_result_layout {
        value_type value;
        unsigned int flags;
        error_type error;
      };
      exception_type exception;
    };
    ```
    If you choose standard layout `value_type`, `error_type` and `exception_type`, `basic_outcome`
works fine from C on MSVC, GCC and clang.
- `EqualityComparable`, if all of `value_type`, `error_type` and `exception_type` implement equality comparisons with one another.
- ~~`LessThanComparable`~~, not implemented due to availability of implicit conversions from `value_type`, `error_type` and `exception_type`, this can cause major surprise (i.e. hard to diagnose bugs), so we don't implement these at all.
- ~~`Hash`~~, not implemented as a generic implementation of a unique hash for non-valued items which are unequal would require a dependency on RTTI being enabled.

Thus `basic_outcome` meets the `Regular` concept if all of `value_type`, `error_type` and `exception_type` are `Regular`, except for the lack of a default constructor. Often where one needs a default constructor, wrapping `basic_outcome` into {{% api "std::optional<T>" %}} will suffice.

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

