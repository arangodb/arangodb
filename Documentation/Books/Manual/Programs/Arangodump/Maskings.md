Arangodump Data Maskings
========================

*--maskings path-of-config*

It is possible to mask certain fields during dump. A JSON config file is
used to define which fields should be masked and how.

The general structure of the config file is

```json
{
  "collection-name": {
    "type": MASKING_TYPE
    "maskings" : [
      MASKING1,
      MASKING2,
      ...
    ]
  },
  ...
}
```

Using `"*"` as collection name defines a default behavior for collections not
listed explicitly.

Masking Types
-------------

This is a string describing how to mask this collection. Possible values are

- "exclude": the collection is ignored completely and not even the structure data
  is dumped.

- "structure": only the collection structure is dumped, but no data at all

- "masked": the collection structure and all data is dumped. However, the data
  is subject to maskings defined in the attribute maskings.

- "full": the collection structure and all data is dumped. No masking at all
  is done for this collection.

For example:

```json
{
  "private": {
    "type": "exclude"
  },

  "log": {
    "type": "structure"
  },

  "person": {
    "type": "masked",
    "maskings": [
      {
        "path": "name",
        "type": "xifyFont",
        "unmaskedLength": 2
      },
      {
        "path": ".security_id",
        "type": "xifyFont",
        "unmaskedLength": 2
      }
    ]
  }
}
```

In the example the collection "private" is completely ignored. Only the
structure of the collection "log" is dumped, but not the data itself.
The collection "person" is dumped completely but masking the "name" field if
it occurs on the top-level. It masks the field "security_id" anywhere in the
document. See below for a complete description of the parameters of
"xifyFont".

### Masking vs. dump-data option

*arangodump* also supports a very coarse masking with the option
`--dump-data false`.  This basically removes all data from the dump.

You can either use `--masking` or `--dump-data false`, but not both.

### Masking vs. include-collection option

*arangodump* also supports a very coarse masking with the option
`--include-collection`. This will restrict the collections that are
dumped to the ones explicitly listed..

It is possible to combine `--masking` and `--include-collection`.
This will take the intersection of exportable collections.

Path
----

If the path starts with a `.` then it is considered to match any path
ending in `name`. For example, `.name` will match the attribute name
`name` all leaf attributes in the document. Leaf attributes are
attributes whose value is `null` or of data type `string`, `number`,
`bool` and `array` (see below). `name` will only match leaf attributes
at top level. `person.name` will match the attribute `name` of a leaf
in the top-level object `person`.

If you have a attribute name that contains a dot, you need to quote the
name with either a tick or a backtick. For example

    "path": "´name.with.dots´"

or

    "path": "`name.with.dots`"

If the attribute value is an array the masking is applied to all the
array elements individually.

**Example**

The following configuration will replace the value of the "name"
attribute with an "XXXX"-masked string:

```json
{
  "type": "xifyFont",
  "path": ".name",
  "unmaskedLength": 2
}
```

The document:

```json
{
  "name": "top-level-name",
  "age": 42,
  "nicknames" : [ { "name": "hugo" }, "egon" ],
  "other": {
    "name": [ "emil", { "secret": "superman" } ]
  }
}
```

will be changed as follows:

```json
{
  "name": "xxxxxxxxxxxxme",
  "age": 42,
  "nicknames" : [ { "name": "xxgo" }, "egon" ],
  "other": {
    "name": [ "xxil", { "secret": "superman" } ]
  }
}
```

The values `"egon"` and `"superman"` are not replaced, because they
are not contained in an attribute value of which the attribute name is
`name`.

### Nested objects and arrays

If you specify a path and the attribute value is an array then the
masking decision is applied to each element of the array as if this
was the value of the attribute.

If the attribute value is an object, then the attribute is not masked.
Instead the nested object is checked further for leaf attributes.

**Example**

Masking `email` will convert:


```json
{ 
  "email" : "email address" 
}
```

into:

```json
{ 
  "email" : "xxil xxxxxxss" 
}
```

because `email` is a leaf attribute.

```json
{ 
  "email" : [ 
    "address one", 
    "address two" 
  ] 
} 
```

will be converted into:

```json
{ 
  "email" : [ 
    "xxxxxss xne", 
    "xxxxxss xwo" 
  ] 
} 
```

because the array is "unfolded".

```json
{ 
  "email" : { 
    "address" : "email address" 
  } 
}
```

will not be changed because `email` is not a leaf attribute.


Masking Functions
-----------------

### randomString

This masking replaces character strings with a random string of the
same length as the input. Other values, e.g. numbers and booleans, are
not changed.

```json
{
  "path": ".name",
  "type": "randomString"
}
```

This will replace all values of attributes with key `name` with an
anonymized string. It is not guaranteed that the string will be of
the same length.

A hash of the original string is computed. If the original string is
shortened then the hash will be used. This will result in a longer
replacement string. If the string is longer than the hash then
characters will be repeated as many times as needed to reach the full
original string length.

### xifyFont (Enterprise)

This masking replaces characters with `x` and blanks. Alphanumeric characters,
`_` and `-` are replaced by `x`, everything else is replaced by a blank.

```json
{
  "path": ".name",
  "type": "xifyFont",
  "unmaskedLength": 2
}
```

This will mask all alphanumeric characters of a word except the last 2.
Words of length 1 and 2 are unmasked. If the attribute value is not a
string the result will be `xxxx`.

    "This is a test!Do you agree?"

will become

    "xxis is a xxst Do xou xxxee "

There is a catch. If you have an index on the attribute the masking
might distort the index efficiency or even cause errors in case of a
unique index.

```json
{
  "type": "xifyFont",
  "path": ".name",
  "unmaskedLength": 2,
  "hash": true
}
```

This will add a hash at the end of the string.

    "This is a test!Do you agree?"

will become

    "xxis is a xxst Do xou xxxee  NAATm8c9hVQ="

Note that the hash is based on a random secrect that is different for
each run. This avoids dictionary attacks.

If you need reproducable results, i.e. hashes that do not change between
different runs of *arangodump*, you need to specify a secret as seed,
a number which must not be `0`.

```json
{
  "type": "xifyFont",
  "path": ".name",
  "unmaskedLength": 2,
  "hash": true,
  "seed": 246781478647
}
```

### zip (Enterprise)

This masking replaces a zip code with a random one.  If the attribute
value is not a string then the default value of `12345` is used.

```json
{
  "path": ".code",
  "type": "zip",
}
```

This will replace an existing zip with a random one. It uses the following
rule: If a character of the original zip code is a digit it will be replaced
by a random digit. If a character of the original zip code is a letter it
will be replaced by a random letter keeping the case.

```json
{
  "path": ".code",
  "type": "zip",
  "default": "abcdef"
}
```

**Example**

If the original zip code is

    50674

it will be replaced by (for example)

    98146

If the original zip code is

    SA34-EA

it will be replaced by (for example)

    OW91-JI

Please note that this will generate random zip code value. Therefore
there is a chance that it will destroy uniqueness.

### datetime (Enterprise)

This masking replaces the value of the attribute with a random date.

```json
{
  "type": "datetime",
  "begin" : "2019-01-01",
  "end": "2019-12-31",
  "format": "%yyyy-%mm-%dd",
}
```

`begin` and `end` are in ISO8601.

The format is described in
[DATE_FORMAT](https://docs.arangodb.com/3.4/AQL/Functions/Date.html#dateformat).

### integral number (Enterprise)

This masking replaces the value of the attribute with a random integral number.

```json
{
  "type": "integer",
  "lower" : -100,
  "upper": 100
}
```

### decimal (Enterprise)

This masking replaces the value of the attribute with a random decimal.

```json
{
  "type": "float",
  "lower" : -0.3,
  "upper": 0.3
}
```

by default, the decimal has a scale of 2.

```json
{
  "type": "float",
  "lower" : -0.3,
  "upper": 0.3,
  "scale": 3
}
```

will use a scale of 3.

### Credit card number (Enterprise)

This masking replaces the value of the attribute with a random credit card number.

```json
{
  "type": "creditCard",
}
```

See [Luhn](https://en.wikipedia.org/wiki/Luhn_algorithm) for details.

### Phone number (Enterprise)

This masking replaces a phone number with a random one. If the attribute value
is not a string it is replaced by the string `"+1234567890"`.

```json
{
  "type": "phone",
  "default": "+4912345123456789"
}
```

This will replace an existing phone number with a random one. It uses
the following rule: If a character of the original number is a digit
it will be replaced by a random digit. If it is letter it is replaced
by a letter. All other characters are unchanged.

```json
{
  "type": "zip",
  "default": "+4912345123456789"
}
```

If the attribute value is not a string use the value of default
`"+4912345123456789"`.

### Email address (Enterprise)

This takes an email address, computes a hash value and split this is
three equal parts `AAAA`, `BBBB`, and `CCCC`. The resulting email
address is `AAAA.BBBB@CCCC.invalid`.
