+++
title = "Constrained template macros"
+++

*Overridable*: All of the following macros are overridable, define before inclusion.

*Header*: `<boost/outcome/config.hpp>`

These macros expand into either the syntax for directly specifying constrained templates in C++ 20, or into a SFINAE based emulation for earlier C++ versions. Form of usage looks as follows:

```c++
BOOST_OUTCOME_TEMPLATE(class ErrorCondEnum)
  BOOST_OUTCOME_TREQUIRES(
    // If this is a valid expression
    BOOST_OUTCOME_TEXPR(error_type(make_error_code(ErrorCondEnum()))),
    // If this predicate is true
    BOOST_OUTCOME_TPRED(predicate::template enable_error_condition_converting_constructor<ErrorCondEnum>)
    // Any additional requirements follow here ...
  )
  constexpr basic_result(ErrorCondEnum &&t, error_condition_converting_constructor_tag /*unused*/ = {});
```

Be aware that slightly different semantics occur for real C++ 20 constrained templates than for the SFINAE emulation.

- <a name="template"></a>`BOOST_OUTCOME_TEMPLATE(template args ...)`

    Begins a constrained template declaration.

- <a name="trequires"></a>`BOOST_OUTCOME_TREQUIRES(requirements ...)`

    Specifies the requirements for the constrained template to be available for selection by the compiler.

- <a name="texpr"></a>`BOOST_OUTCOME_TEXPR(expression)`

    A requirement that the given expression is valid.

- <a name="tpred"></a>`BOOST_OUTCOME_TPRED(boolean)`

    A requirement that the given constant time expression is true.
