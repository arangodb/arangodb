

@brief optional arguments to pass to v8
`--javascript.v8-options options`

Optional arguments to pass to the V8 Javascript engine. The V8 engine will
run with default settings unless explicit options are specified using this
option. The options passed will be forwarded to the V8 engine which will
parse them on its own. Passing invalid options may result in an error
being
printed on stderr and the option being ignored.

Options need to be passed in one string, with V8 option names being
prefixed
with double dashes. Multiple options need to be separated by whitespace.
To get a list of all available V8 options, you can use
the value *"--help"* as follows:
```
--javascript.v8-options "--help"
```

Another example of specific V8 options being set at startup:

```
--javascript.v8-options "--harmony --log"
```

Names and features or usable options depend on the version of V8 being
used,
and might change in the future if a different version of V8 is being used
in ArangoDB. Not all options offered by V8 might be sensible to use in the
context of ArangoDB. Use the specific options only if you are sure that
they are not harmful for the regular database operation.

