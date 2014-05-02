# Error Handling

Many functions in Libbson can result in errors.
To simplify this, Libbson contains a structure called `bson_error_t`.
This structure enapsulates an error from the underlying system and can be passed back up through the stack.

The structure contains a `domain` and `code` field that can be used to check for specific error conditions.
The value of these fields is API specific.
If you simply want to print an error message, use the `message` field of `bson_error_t`.

You do not need to destroy this structure.
For those who use `GLib` and `GError`, this may feel unnatural.
However, since our error cases are non-generic, it is preferred to additional memory allocations.
