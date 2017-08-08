String functions
================

For string processing, AQL offers the following functions:

### CHAR_LENGTH()

`CHAR_LENGTH(value) → length`

Return the number of characters in *value* (not byte length).

|input|length|
|---|---|
|String|number of unicode characters|
|Number|number of unicode characters that represent the number|
|Array / Object|number of unicode characters from the resulting stringification|
|true| 4 |
|false| 5 |
|null| 0 |

### CONCAT()

`CONCAT(value1, value2, ... valueN) → str`

Concatenate the values passed as *value1* to *valueN*.

- **values** (any, *repeatable*): elements of arbitrary type (at least 1)
- returns **str** (string): a concatenation of the elements. *null* values
  are ignored.

```js
CONCAT("foo", "bar", "baz") // "foobarbaz"
CONCAT(1, 2, 3) // "123"
CONCAT("foo", [5, 6], {bar: "baz"}) // "foo[5,6]{\"bar\":\"baz\"}"
```

`CONCAT(anyArray) → str`

If a single array is passed to *CONCAT()*, its members are concatenated.

- **anyArray** (array): array with elements of arbitrary type
- returns **str** (string): a concatenation of the array elements. *null* values
  are ignored.

```js
CONCAT( [ "foo", "bar", "baz" ] ) // "foobarbaz"
CONCAT( [1, 2, 3] ) // "123"
```

### CONCAT_SEPARATOR()

`CONCAT_SEPARATOR(separator, value1, value2, ... valueN) → joinedString`

Concatenate the strings passed as arguments *value1* to *valueN* using the
*separator* string.

- **separator** (string): an arbitrary separator string
- **values** (string|array, *repeatable*): strings or arrays of strings as multiple
  arguments (at least 1)
- returns **joinedString** (string): a concatenated string of the elements, using
  *separator* as separator string. *null* values are ignored. Array value arguments
  are expanded automatically, and their individual members will be concatenated.
  Nested arrays will be expanded too, but with their elements separated by commas
  if they have more than a single element.

```js
CONCAT_SEPARATOR(", ", "foo", "bar", "baz")
// "foo, bar, baz"

CONCAT_SEPARATOR(", ", [ "foo", "bar", "baz" ])
// "foo, bar, baz"

CONCAT_SEPARATOR(", ", [ "foo", [ "b", "a", "r" ], "baz" ])
// [ "foo, b,a,r, baz" ]

CONCAT_SEPARATOR("-", [1, 2, 3, null], [4, null, 5])
// "1-2-3-4-5"
```

### CONTAINS()

`CONTAINS(text, search, returnIndex) → match`

Check whether the string *search* is contained in the string *text*.
The string matching performed by *CONTAINS* is case-sensitive.

- **text** (string): the haystack
- **search** (string): the needle
- **returnIndex** (bool, *optional*): if set to *true*, the character position
  of the match is returned instead of a boolean. The default is *false*.
  The default is *false*.
- returns **match** (bool|number): by default, *true* is returned if *search*
  is contained in *text*, and *false* otherwise. With *returnIndex* set to *true*,
  the position of the first occurrence of *search* within *text* is returned 
  (starting at offset 0), or *-1* if *search* is not contained in *text*.

```js
CONTAINS("foobarbaz", "bar") // true
CONTAINS("foobarbaz", "horse") // false
CONTAINS("foobarbaz", "ba", true) // 3
CONTAINS("foobarbaz", "horse", true) // -1
```

### COUNT()

This is an alias for [LENGTH()](#length).

### FIND_FIRST()

`FIND_FIRST(text, search, start, end) → position`

Return the position of the first occurrence of the string *search* inside the
string *text*. Positions start at 0.

- **text** (string): the haystack
- **search** (string): the needle
- **start** (number, *optional*): limit the search to a subset of the text,
  beginning at *start*
- **end** (number, *optional*): limit the search to a subset of the text,
  ending at *end*
- returns **position** (number): the character position of the match. If *search*
  is not contained in *text*, -1 is returned.

```js
FIND_FIRST("foobarbaz", "ba") // 3
FIND_FIRST("foobarbaz", "ba", 4) // 6
FIND_FIRST("foobarbaz", "ba", 0, 3) // -1
```

### FIND_LAST()

`FIND_LAST(text, search, start, end) → position`

Return the position of the last occurrence of the string *search* inside the
string *text*. Positions start at 0.

- **text** (string): the haystack
- **search** (string): the needle
- **start** (number, *optional*): limit the search to a subset of the text,
  beginning at *start*
- **end** (number, *optional*): limit the search to a subset of the text,
  ending at *end*
- returns **position** (number): the character position of the match. If *search*
  is not contained in *text*, -1 is returned.

```js
FIND_LAST("foobarbaz", "ba") // 6
FIND_LAST("foobarbaz", "ba", 7) // -1
FIND_LAST("foobarbaz", "ba", 0, 4) // 3
```

### JSON_PARSE()

`JSON_PARSE(text) → value`

Return an AQL value described by the JSON-encoded input string.

- **text** (string): the string to parse as JSON
- returns **value** (mixed): the value corresponding to the given JSON text. 
  For input values that are no valid JSON strings, the function will return *null*.

```js
JSON_PARSE("123") // 123
JSON_PARSE("[ true, false, 2 ]") // [ true, false, 2 ]
JSON_PARSE("\\\"abc\\\"") // "abc"
JSON_PARSE("{\\\"a\\\": 1}") // { a : 1 }
JSON_PARSE("abc") // null
```

### JSON_STRINGIFY()

`JSON_STRINGIFY(value) → text`

Return a JSON string representation of the input value.

- **value** (mixed): the value to convert to a JSON string
- returns **text** (string): the JSON string representing *value*.
  For input values that cannot be converted to JSON, the function 
  will return *null*.

```js
JSON_STRINGIFY("1") // "1"
JSON_STRINGIFY("abc") // "\"abc\""
JSON_STRINGIFY("[1, 2, 3]") // "[1,2,3]"
```

### LEFT()

`LEFT(value, length) → substring`

Return the *length* leftmost characters of the string *value*.

- **value** (string): a string
- **length** (number): how many characters to return
- returns **substring** (string): at most *length* characters of *value*,
  starting on the left-hand side of the string

```js
LEFT("foobar", 3) // "foo"
LEFT("foobar", 10) // "foobar"
```

### LENGTH()

`LENGTH(str) → length`

Determine the character length of a string.

- **str** (string): a string. If a number is passed, it will be casted to string first.
- returns **length** (number): the character length of *str* (not byte length)

```js
LENGTH("foobar") // 6
LENGTH("电脑坏了") // 4
```

*LENGTH()* can also determine the [number of elements](Array.md#length) in an array,
the [number of attribute keys](Document.md#length) of an object / document and
the [amount of documents](Miscellaneous.md#length) in a collection.

### LIKE()

`LIKE(text, search, caseInsensitive) → bool`

Check whether the pattern *search* is contained in the string *text*,
using wildcard matching. 

- **text** (string): the string to search in
- **search** (string): a search pattern that can contain the wildcard characters
  `%` (meaning any sequence of characters, including none) and `_` (any single
  character). Literal *%* and *:* must be escaped with two backslashes (four
  in arangosh).
  *search* cannot be a variable or a document attribute. The actual value must
  be present at query parse time already.
- **caseInsensitive** (bool, *optional*): if set to *true*, the matching will be
  case-insensitive. The default is *false*.
- returns **bool** (bool): *true* if the pattern is contained in *text*,
  and *false* otherwise

```js
LIKE("cart", "ca_t")   // true
LIKE("carrot", "ca_t") // false
LIKE("carrot", "ca%t") // true

LIKE("foo bar baz", "bar")   // false
LIKE("foo bar baz", "%bar%") // true
LIKE("bar", "%bar%")         // true

LIKE("FoO bAr BaZ", "fOo%bAz")       // false
LIKE("FoO bAr BaZ", "fOo%bAz", true) // true
```

### LOWER()

`LOWER(value) → lowerCaseString`

Convert upper-case letters in *value* to their lower-case counterparts.
All other characters are returned unchanged.

- **value** (string): a string
- returns **lowerCaseString** (string): *value* with upper-case characters converted
  to lower-case characters

### LTRIM()

`LTRIM(value, chars) → strippedString`

Return the string *value* with whitespace stripped from the start only.

- **value** (string): a string
- **chars** (string, *optional*): override the characters that should
  be removed from the string. It defaults to `\r\n \t` (i.e. `0x0d`, `0x0a`,
  `0x20` and `0x09`).
- returns **strippedString** (string): *value* without *chars* at the
  left-hand side

```js
LTRIM("foo bar") // "foo bar"
LTRIM("  foo bar  ") // "foo bar  "
LTRIM("--==[foo-bar]==--", "-=[]") // "foo-bar]==--"
```

### MD5()

`MD5(text) → hash`

Calculate the MD5 checksum for *text* and return it in a hexadecimal
string representation.

- **text** (string): a string
- returns **hash** (string): MD5 checksum as hex string

```js
MD5("foobar") // "3858f62230ac3c915f300c664312c63f"
```

### RANDOM_TOKEN()

`RANDOM_TOKEN(length) → randomString`

Generate a pseudo-random token string with the specified length.
The algorithm for token generation should be treated as opaque.

- **length** (number): desired string length for the token. It must be greater
  than 0 and at most 65536.
- returns **randomString** (string): a generated token consisting of lowercase
  letters, uppercase letters and numbers

```js
RANDOM_TOKEN(8) // "zGl09z42"
RANDOM_TOKEN(8) // "m9w50Ft9"
```

### REGEX_TEST()

`REGEX_TEST(text, search, caseInsensitive) → bool`

Check whether the pattern *search* is contained in the string *text*,
using regular expression matching.

- **text** (string): the string to search in
- **search** (string): a regular expression search pattern
- returns **bool** (bool): *true* if the pattern is contained in *text*,
  and *false* otherwise

The regular expression may consist of literal characters and the following 
characters and sequences:

- `.` – the dot matches any single character except line terminators.
  To include line terminators, use `[\s\S]` instead to simulate `.` with *DOTALL* flag.
- `\d` – matches a single digit, equivalent to `[0-9]`
- `\s` – matches a single whitespace character
- `\S` – matches a single non-whitespace character
- `\t` – matches a tab character
- `\r` – matches a carriage return
- `\n` – matches a line-feed character
- `[xyz]` – set of characters. matches any of the enclosed characters (i.e.
  *x*, *y* or *z* in this case
- `[^xyz]` – negated set of characters. matches any other character than the
  enclosed ones (i.e. anything but *x*, *y* or *z* in this case)
- `[x-z]` – range of characters. Matches any of the characters in the 
  specified range, e.g. `[0-9A-F]` to match any character in
  *0123456789ABCDEF*
- `[^x-z]` – negated range of characters. Matches any other character than the
  ones specified in the range
- `(xyz)` – defines and matches a pattern group
- `(x|y)` – matches either *x* or *y*
- `^` – matches the beginning of the string (e.g. `^xyz`)
- <code>$</code> – matches the end of the string (e.g. <code>xyz$</code>)

Note that the characters `.`, `*`, `?`, `[`, `]`, `(`, `)`, `{`, `}`, `^`, 
and `$` have a special meaning in regular expressions and may need to be 
escaped using a backslash, which requires escaping itself (`\\`). A literal
backslash needs to be escaped using another escaped backslash, i.e. `\\\\`.
In arangosh, the amount of backslashes needs to be doubled.

Characters and sequences may optionally be repeated using the following
quantifiers:

- `x*` – matches zero or more occurrences of *x*
- `x+` – matches one or more occurrences of *x*
- `x?` – matches one or zero occurrences of *x*
- `x{y}` – matches exactly *y* occurrences of *x*
- `x{y,z}` – matches between *y* and *z* occurrences of *x*
- `x{y,}` – matches at least *y* occurences of *x*

Note that `xyz+` matches *xyzzz*, but if you want to match *xyzxyz* instead,
you need to define a pattern group by wrapping the subexpression in parentheses
and place the quantifier right behind it: `(xyz)+`.
 
If the regular expression in *search* is invalid, a warning will be raised
and the function will return *false*.

```js
REGEX_TEST("the quick brown fox", "the.*fox") // true
REGEX_TEST("the quick brown fox", "^(a|the)\s+(quick|slow).*f.x$") // true
REGEX_TEST("the\nquick\nbrown\nfox", "^the(\n[a-w]+)+\nfox$") // true
```

### REGEX_REPLACE()

`REGEX_REPLACE(text, search, replacement, caseInsensitive) → string`

Replace the pattern *search* with the string *replacement* in the string
*text*, using regular expression matching.

- **text** (string): the string to search in
- **search** (string): a regular expression search pattern
- **replacement** (string): the string to replace the *search* pattern with
- returns **string** (string): the string *text* with the *search* regex
  pattern replaced with the *replacement* string wherever the pattern exists
  in *text*

For more details about the rules for characters and sequences refer
[REGEX_TEST()](#regextest).
 
If the regular expression in *search* is invalid, a warning will be raised
and the function will return *false*.

```js
REGEX_REPLACE("the quick brown fox", "the.*fox", "jumped over") // jumped over
REGEX_REPLACE("the quick brown fox", "o", "i") // the quick briwn fix
```

### REVERSE()

`REVERSE(value) → reversedString`

Return the reverse of the string *str*.

- **value** (string): a string
- returns **reversedString** (string): a new string with the characters in
  reverse order

```js
REVERSE("foobar") // "raboof"
REVERSE("电脑坏了") // "了坏脑电"
```

### RIGHT()

`RIGHT(value, length) → substring`

Return the *length* rightmost characters of the string *value*.

- **value** (string): a string
- **length** (number): how many characters to return
- returns **substring** (string): at most *length* characters of *value*,
  starting on the right-hand side of the string

```js
RIGHT("foobar", 3) // "bar"
RIGHT("foobar", 10) // "foobar"
```

### RTRIM()

`RTRIM(value, chars) → strippedString`

Return the string *value* with whitespace stripped from the end only.

- **value** (string): a string
- **chars** (string, *optional*): override the characters that should
  be removed from the string. It defaults to `\r\n \t` (i.e. `0x0d`, `0x0a`,
  `0x20` and `0x09`).
- returns **strippedString** (string): *value* without *chars* at the
  right-hand side

```js
RTRIM("foo bar") // "foo bar"
RTRIM("  foo bar  ") // "  foo bar"
RTRIM("--==[foo-bar]==--", "-=[]") // "--==[foo-bar"
```

### SHA1()

`SHA1(text) → hash`

Calculate the SHA1 checksum for *text* and returns it in a hexadecimal
string representation.

- **text** (string): a string
- returns **hash** (string): SHA1 checksum as hex string

```js
SHA1("foobar") // "8843d7f92416211de9ebb963ff4ce28125932878"
```

### SPLIT()

`SPLIT(value, separator, limit) → strArray`

Split the given string *value* into a list of strings, using the *separator*.

- **value** (string): a string
- **separator** (string): either a string or a list of strings. If *separator* is
  an empty string, *value* will be split into a list of characters. If no *separator*
  is specified, *value* will be returned as array.
- **limit** (number, *optional*): limit the number of split values in the result.
  If no *limit* is given, the number of splits returned is not bounded.
- returns **strArray** (array): an array of strings

```js
SPLIT( "foo-bar-baz", "-" ) // [ "foo", "bar", "baz" ]
SPLIT( "foo-bar-baz", "-", 1 ) // [ "foo", "bar-baz" ]
SPLIT( "foo, bar & baz", [ ", ", " & " ] ) // [ "foo", "bar", "baz" ]
```

### SUBSTITUTE()

`SUBSTITUTE(value, search, replace, limit) → substitutedString`

Replace search values in the string *value*.

- **value** (string): a string
- **search** (string|array): if *search* is a string, all occurrences of
  *search* will be replaced in *value*. If *search* is an array of strings,
  each occurrence of a value contained in *search* will be replaced by the
  corresponding array element in *replace*. If *replace* has less list items
  than *search*, occurrences of unmapped *search* items will be replaced by an
  empty string.
- **replace** (string|array, *optional*): a replacement string, or an array of
  strings to replace the corresponding elements of *search* with. Can have less
  elements than *search* or be left out to remove matches. If *search* is an array
  but *replace* is a string, then all matches will be replaced with *replace*.
- **limit** (number, *optional*): cap the number of replacements to this value
- returns **substitutedString** (string): a new string with matches replaced
  (or removed)

```js
SUBSTITUTE( "the quick brown foxx", "quick", "lazy" )
// "the lazy brown foxx"

SUBSTITUTE( "the quick brown foxx", [ "quick", "foxx" ], [ "slow", "dog" ] )
// "the slow brown dog"

SUBSTITUTE( "the quick brown foxx", [ "the", "foxx" ], [ "that", "dog" ], 1 )
//  "that quick brown foxx"

SUBSTITUTE( "the quick brown foxx", [ "the", "quick", "foxx" ], [ "A", "VOID!" ] )
// "A VOID! brown "

SUBSTITUTE( "the quick brown foxx", [ "quick", "foxx" ], "xx" )
// "the xx brown xx"
```

`SUBSTITUTE(value, mapping, limit) → substitutedString`

Alternatively, *search* and *replace* can be specified in a combined value.

- **value** (string): a string
- **mapping** (object): a lookup map with search strings as keys and replacement
  strings as values. Empty strings and *null* as values remove matches.
- **limit** (number, *optional*): cap the number of replacements to this value
- returns **substitutedString** (string): a new string with matches replaced
  (or removed)

```js
SUBSTITUTE("the quick brown foxx", {
  "quick": "small",
  "brown": "slow",
  "foxx": "ant"
})
// "the small slow ant"

SUBSTITUTE("the quick brown foxx", { 
  "quick": "",
  "brown": null,
  "foxx": "ant"
})
// "the   ant"

SUBSTITUTE("the quick brown foxx", {
  "quick": "small",
  "brown": "slow",
  "foxx": "ant"
}, 2)
// "the small slow foxx"
```

### SUBSTRING()

`SUBSTRING(value, offset, length) → substring`

Return a substring of *value*.

- **value** (string): a string
- **offset** (number): start at *offset*, offsets start at position 0
- **length** (number, *optional*): at most *length* characters, omit to get the
  substring from *offset* to the end of the string
- returns **substring** (string): a substring of *value*

### TRIM()

`TRIM(value, type) → strippedString`

Return the string *value* with whitespace stripped from the start and/or end.

The optional *type* parameter specifies from which parts of the string the
whitespace is stripped. [LTRIM()](#ltrim) and [RTRIM()](#rtrim) are preferred
however.

- **value** (string): a string
- **type** (number, *optional*): strip whitespace from the
  - 0 – start and end of the string
  - 1 – start of the string only
  - 2 – end of the string only
  The default is 0.

`TRIM(value, chars) → strippedString`

Return the string *value* with whitespace stripped from the start and end.

- **value** (string): a string
- **chars** (string, *optional*): override the characters that should
  be removed from the string. It defaults to `\r\n \t` (i.e. `0x0d`, `0x0a`,
  `0x20` and `0x09`).
- returns **strippedString** (string): *value* without *chars* on both sides

```js
TRIM("foo bar") // "foo bar"
TRIM("  foo bar  ") // "foo bar"
TRIM("--==[foo-bar]==--", "-=[]") // "foo-bar"  
TRIM("  foobar\t \r\n ") // "foobar"
TRIM(";foo;bar;baz, ", ",; ") // "foo;bar;baz"
```

### UPPER()

`UPPER(value) → upperCaseString`

Convert lower-case letters in *value* to their upper-case counterparts.
All other characters are returned unchanged.

- **value** (string): a string
- returns **upperCaseString** (string): *value* with lower-case characters converted
  to upper-case characters
