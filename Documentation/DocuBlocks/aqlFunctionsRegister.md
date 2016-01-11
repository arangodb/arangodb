

@brief register an AQL user function
`aqlfunctions.register(name, code, isDeterministic)`

Registers an AQL user function, identified by a fully qualified function
name. The function code in *code* must be specified as a JavaScript
function or a string representation of a JavaScript function.
If the function code in *code* is passed as a string, it is required that
the string evaluates to a JavaScript function definition.

If a function identified by *name* already exists, the previous function
definition will be updated. Please also make sure that the function code
does not violate the [Conventions](../AqlExtending/Conventions.md) for AQL 
functions.

The *isDeterministic* attribute can be used to specify whether the
function results are fully deterministic (i.e. depend solely on the input
and are the same for repeated calls with the same input values). It is not
used at the moment but may be used for optimizations later.

The registered function is stored in the selected database's system 
collection *_aqlfunctions*.

The function returns *true* when it updates/replaces an existing AQL 
function of the same name, and *false* otherwise. It will throw an exception
when it detects syntactially invalid function code.

@EXAMPLES

```js
  require("@arangodb/aql/functions").register("myfunctions::temperature::celsiustofahrenheit",
  function (celsius) {
    return celsius * 1.8 + 32;
  });
```

