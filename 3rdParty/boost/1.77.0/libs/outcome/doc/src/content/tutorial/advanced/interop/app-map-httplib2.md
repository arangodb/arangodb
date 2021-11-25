+++
title = "Mapping the HTTP library into the Application `2/2`"
weight = 35
+++

If you remember the tutorial section on the [`value_or_error` Concept](../value-or-error),
this is an example of how to implement a custom `value_or_error` Concept converter
in Outcome:

{{% snippet "finale.cpp" "app_map_httplib2" %}}

The first thing that you should note is that these custom converters must be injected
directly into the `BOOST_OUTCOME_V2_NAMESPACE::convert` namespace, and they must partially
or completely specialise {{% api "value_or_error<T, U>" %}}. Here we specialise the
converter for `value_or_error` conversions from `httplib::result<U>` to `app::outcome<T>`
i.e. from our third party HTTP library's error type into our application's `outcome`
type (which is unique to our application, as we hard code an `app`-local error type).

The second thing to note is that you need to set `enable_result_inputs` and `enable_outcome_inputs`
appropriately, otherwise `result` and `outcome` inputs will not be matched by this
converter[^1]. In this converter, we really do wish to convert other `result` and
`outcome` inputs, so we mark these booleans as `true`.

The third thing to note is the requirements on `operator()`. If the requirements are
not met, the `value_or_error` converting constructor in `basic_result` and `basic_outcome`
disables. Note the requirement that the decayed `operator()` input `X` matches
`httplib::result<U>`, and that `T` is constructible from `U`. This means that the
{{% api "explicit basic_result(concepts::value_or_error<T, E> &&)" %}} and {{% api "explicit basic_outcome(concepts::value_or_error<T, E> &&)" %}}
constructors are available if, and only if, the input type is a `httplib::result<U>`,
and the result's value type is constructible from the input's value type.

If `operator()` is available, it naturally converts a `httplib::result<U>` into an
`app::outcome<T>` by either forwarding any success as-is, or calling `app::make_httplib_exception()`
to type erase the `httplib::failure` into an `app::httplib_error`.

[^1]: Here we refer to `result` and `outcome` as defined by this specific Outcome library. If `result` or `outcome` from another Outcome implementation is seen, those **always** must get parsed via the `ValueOrError` matching conversion framework.
