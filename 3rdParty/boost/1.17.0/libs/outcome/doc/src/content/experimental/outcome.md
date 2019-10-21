+++
title = "Tying it all together"
weight = 80
+++

Firstly let's alias a more convenient form of `status_result`:

{{% snippet "experimental_status_code.cpp" "typedef" %}}

(The defaulting of `default_result_policy` is superfluous, it's already the default)

What follows now is very standard Outcome code. Indeed, it would compile
just fine under standard Outcome with only a few typedefs.

{{% snippet "experimental_status_code.cpp" "open_file" %}}

And running this program yields:

```
Returned error has a code domain of 'file i/o error domain', a message of 'No such file or directory (c:\users\ned\documents\boostish\outcome\doc\src\snippets\experimental_status_code.cpp:195)'

And semantically comparing it to 'errc::no_such_file_or_directory' = 1
```

### Conclusion

Once you get used to `<system_error2>` and the fact that any `result` with
`E = error` is always move-only, using experimental Outcome is just like
using normal Outcome. Except that codegen will be better, custom domains
are safe to use in headers, semantic comparisons have guaranteed complexity
bounds, and build times are much reduced.

What's not to like? :)

Finally, if you have feedback on using experimental Outcome which you think
would be of use to the standards committee when evaluating possible
implementations of [P0709 *Zero overhead exceptions: Throwing values*](http://wg21.link/P0709),
please do get in touch! This **especially** includes successful experiences!!!
