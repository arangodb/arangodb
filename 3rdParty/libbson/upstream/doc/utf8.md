# UTF-8

Libbson expects that you are always working with UTF-8 encoded text.
Anything else is <em>INVALID API use</em>.

If you should need to walk through UTF-8 sequences, you can use the various UTF-8 helper functions distributed with Libbson.

# Validating a UTF-8 sequence

To validate the string contained in `my_string`, use the following.
It expects that `my_string` is a standard NUL-terminated C string.
You may pass `-1` for the string length if you know the string is NUL-terminated.

```c
if (!bson_utf8_validate(my_string, -1, FALSE)) {
	/* Validation Failure */
}
```

If `my_string` has NUL bytes within the string, you must provide the string length.
Use the follwing format.

```c
if (!bson_utf8_validate(my_string, my_string_len, TRUE)) {
	/* Validation Failure */
}
```
