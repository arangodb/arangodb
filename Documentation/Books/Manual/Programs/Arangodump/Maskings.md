Arangodump Data Maskings
========================

*--maskings path-of-config*

This feature allows you to define how sensitive data shall be dumped.
It is possible to exclude collections entirely, limit the dump to the
structural information of a collection (name, indexes, sharding etc.)
or to obfuscate certain fields for a dump. A JSON configuration file is
used to define which collections and fields to mask and how.

The general structure of the configuration file looks like this:

```json
{
  "collection-name": {
    "type": MASKING_TYPE,
    "maskings": [
      MASKING1,
      MASKING2,
      ...
    ]
  },
  ...
}
```

At the top level, there is an object with collection names and the masking
settings to be applied to them. Using `"*"` as collection name defines a
default behavior for collections not listed explicitly.

Masking Types
-------------

`type` is a string describing how to mask the given collection.
Possible values are:

- `"exclude"`: the collection is ignored completely and not even the
  structure data is dumped.

- `"structure"`: only the collection structure is dumped, but no data at all

- `"masked"`: the collection structure and all data is dumped. However, the data
  is subject to obfuscation defined in the attribute `maskings`. It is an array
  of objects, with one object per field to mask. Each object needs at least a
  `path` and a `type` attribute to [define which field to mask](#path) and which
  [masking function](#masking-functions) to apply. Depending on the
  masking type, there may exist additional attributes.

- `"full"`: the collection structure and all data is dumped. No masking is
  applied to this collection at all.

**Example**

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
        "type": "xifyFront",
        "unmaskedLength": 2
      },
      {
        "path": ".security_id",
        "type": "xifyFront",
        "unmaskedLength": 2
      }
    ]
  }
}
```

- The collection called _private_ is completely ignored.
- Only the structure of the collection _log_ is dumped, but not the data itself.
- The collection _person_ is dumped completely but with maskings applied:
  - The _name_ field is masked if it occurs on the top-level.
  - It also masks fields with the name _security_id_ anywhere in the document.
  - The masking function is of type [_xifyFront_](#xify-front) in both cases.
    The additional setting `unmaskedLength` is specific so _xifyFront_.

### Masking vs. dump-data option

*arangodump* also supports a very coarse masking with the option
`--dump-data false`. This basically removes all data from the dump.

You can either use `--masking` or `--dump-data false`, but not both.

### Masking vs. include-collection option

*arangodump* also supports a very coarse masking with the option
`--include-collection`. This will restrict the collections that are
dumped to the ones explicitly listed.

It is possible to combine `--masking` and `--include-collection`.
This will take the intersection of exportable collections.

Path
----

`path` defines which field to obfuscate. There can only be a single
path per masking, but an unlimited amount of maskings per collection.

Note that the top-level system attributes like `_key`, `_from` are
never masked.

To mask a top-level attribute value, the path is simply the attribute
name, for instance `"name"` to mask the value `"foobar"`:

```json
{
  "_key": "1234",
  "name": "foobar"
}
```

The path to a nested attribute `name` with a top-level attribute `person`
as its parent is `"person.name"`:

```json
{
  "_key": "1234",
  "person": {
    "name": "foobar"
  }
}
```

If the path starts with a `.` then it matches any path ending in `name`.
For example, `.name` will match the field `name` of all leaf attributes
in the document. Leaf attributes are attributes whose value is `null`,
`true`, `false`, or of data type `string`, `number` or `array`.
That means, it matches `name` at the top level
as well as at any nested level (e.g. `foo.bar.name`), but not nested
objects themselves.

On the other hand, `name` will only match leaf attributes
at top level. `person.name` will match the attribute `name` of a leaf
in the top-level object `person`. If `person` was itself an object,
then the masking settings for this path would be ignored, because it
is not a leaf attribute.

If the attribute value is an **array** then the masking is applied to
**all array elements individually**.

If you have an attribute name that contains a dot, you need to quote the
name with either a tick or a backtick. For example:

    "path": "´name.with.dots´"

or

    "path": "`name.with.dots`"

**Example**

The following configuration will replace the value of the `name`
attribute with an "xxxx"-masked string:

```json
{
  "type": "xifyFront",
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

… will be changed as follows:

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
was the value of the attribute. This applies to arrays inside the array too.

If the attribute value is an object, then it is ignored and the attribute
does not get masked. To mask nested fields, specify the full path for each
leaf attribute.

{% hint 'tip' %}
If some documents have an attribute `email` with a string as value, but other
documents store a nested object under the same attribute name, then make sure
to set up proper masking for the latter case, in which sub-attributes will not
get masked if there is only a masking configured for the attribute `email`
but not its nested attributes.
{% endhint %}

**Examples**

Masking `email` with the _Xify Front_ function will convert:

```json
{
  "email" : "email address"
}
```

… into:

```json
{
  "email" : "xxil xxxxxxss"
}
```

because `email` is a leaf attribute. The document:

```json
{
  "email" : [
    "address one",
    "address two",
    [
      "address three"
    ]
  ]
}
```

… will be converted into:

```json
{
  "email" : [
    "xxxxxss xne",
    "xxxxxss xwo",
    [
      "xxxxxss xxxee"
    ]
  ]
}
```

… because the masking is applied to each array element individually
including the elements of the sub-array. The document:

```json
{
  "email" : {
    "address" : "email address"
  }
}
```

… will not be changed because `email` is not a leaf attribute.
To mask the email address, you could use the paths `email.address`
or `.address`.

### Match all

If the path is `"*"` then this match any leaf attribute.

Masking Functions
-----------------

{% hint 'info' %}
The following masking functions are only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/).
{% endhint %}

- [Xify Front](#xify-front)
- [Zip](#zip)
- [Datetime](#datetime)
- [Integral Number](#integral-number)
- [Decimal Number](#decimal-number)
- [Credit Card Number](#credit-card-number)
- [Phone Number](#phone-number)
- [Email Address](#email-address)

The masking functions:

- [Random String](#random-string)
- [Random](#random)

… are available in the Community Edition as well as the Enterprise Edition.

### Random String

This masking type will replace all values of attributes whose values are strings
with key `name` with an anonymized string. It is not guaranteed that the
string will be of the same length. Attribute whose values are not strings
are not modified.

A hash of the original string is computed. If the original string is
shorter then the hash will be used. This will result in a longer
replacement string. If the string is longer than the hash then
characters will be repeated as many times as needed to reach the full
original string length.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"randomString"`

**Example**

```json
{
  "path": ".name",
  "type": "randomString"
}
```

Above masking setting applies to all leaf attributes with name `.name`.
A document like:

```json
{
  "_key" : "1234",
  "name" : [
    "My Name",
    {
      "other" : "Hallo Name"
    },
    [
      "Name One",
      "Name Two"
    ],
    true,
    false,
    null,
    1.0,
    1234,
    "This is a very long name"
  ],
  "deeply": {
    "nested": {
      "name": "John Doe",
      "not-a-name": "Pizza"
    }
  }
}
```

… will be converted to:

```json
{
  "_key": "1234",
  "name": [
    "+y5OQiYmp/o=",
    {
      "other": "Hallo Name"
    },
    [
      "ihCTrlsKKdk=",
      "yo/55hfla0U="
    ],
    true,
    false,
    null,
    1.0,
    1234,
    "hwjAfNe5BGw=hwjAfNe5BGw="
  ],
  "deeply": {
    "nested": {
      "name": "55fHctEM/wY=",
      "not-a-name": "Pizza"
    }
  }
}
```

### Random

This masking type works like random string for attributes with string
values. Values Attributes with integer, decimal or boolean values are
replaced by random integers, decimals or boolean.

### Xify Front

This masking type replaces the front characters with `x` and
blanks. Alphanumeric characters, `_` and `-` are replaced by `x`,
everything else is replaced by a blank.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"xifyFront"`
- `unmaskedLength` (number, _default: `2`_): how many characters to
  leave as-is on the right-hand side of each word as integer value
- `hash` (bool, _default: `false`_): whether to append a hash value to the
  masked string to avoid possible unique constraint violations caused by
  the obfuscation
- `seed` (integer, _default: `0`_): used as secret for computing the hash.
  A value of `0` means a random seed

**Examples**

```json
{
  "path": ".name",
  "type": "xifyFront",
  "unmaskedLength": 2
}
```

This will mask all alphanumeric characters of a word except the last
two characters. Words of length 1 and 2 are unmasked. If the
attribute value is not a string the result will be `xxxx`.

    "This is a test!Do you agree?"

… will become:

    "xxis is a xxst Do xou xxxee "

There is a catch. If you have an index on the attribute the masking
might distort the index efficiency or even cause errors in case of a
unique index.

```json
{
  "type": "xifyFront",
  "path": ".name",
  "unmaskedLength": 2,
  "hash": true
}
```

This will add a hash at the end of the string.

    "This is a test!Do you agree?"

… will become

    "xxis is a xxst Do xou xxxee  NAATm8c9hVQ="

Note that the hash is based on a random secret that is different for
each run. This avoids dictionary attacks which can be used to guess
values based pre-computations on dictionaries.

If you need reproducible results, i.e. hashes that do not change between
different runs of *arangodump*, you need to specify a secret as seed,
a number which must not be `0`.

```json
{
  "type": "xifyFront",
  "path": ".name",
  "unmaskedLength": 2,
  "hash": true,
  "seed": 246781478647
}
```

### Zip

This masking type replaces a zip code with a random one.
It uses the following rules:

- If a character of the original zip code is a digit it will be replaced
  by a random digit.
- If a character of the original zip code is a letter it
  will be replaced by a random letter keeping the case.
- If the attribute value is not a string then the default value is used.

Note that this will generate random zip codes. Therefore there is a
chance that the same zip code value is generated multiple times, which can
cause unique constraint violations if a unique index is or will be
used on the zip code attribute.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"zip"`
- `default` (string, _default: `"12345"`_): if the input field is not of
  data type `string`, then this value is used

**Examples**

```json
{
  "path": ".code",
  "type": "zip",
}
```

This replaces real zip codes stored in fields called `code` at any level
with random ones. `"12345"` is used as fallback value.

```json
{
  "path": ".code",
  "type": "zip",
  "default": "abcdef"
}
```

If the original zip code is:

    50674

… it will be replaced by e.g.:

    98146

If the original zip code is:

    SA34-EA

… it will be replaced by e.g.:

    OW91-JI

If the original zip code is `null`, `true`, `false` or a number, then the
user-defined default value of `"abcdef"` will be used.

### Datetime

This masking type replaces the value of the attribute with a random
date between two configured dates in a customizable format.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"datetime"`
- `begin` (string, _default: `"1970-01-01T00:00:00.000"`_):
  earliest point in time to return. Date time string in ISO 8601 format.
- `end` (string, _default: now_):
  latest point in time to return. Date time string in ISO 8601 format.
  In case a partial date time string is provided (e.g. `2010-06` without day
  and time) the earliest date and time is assumed (`2010-06-01T00:00:00.000`).
  The default value is the current system date and time.
- `format` (string, _default: `""`_): the formatting string format is
  described in [DATE_FORMAT()](../../../AQL/Functions/Date.html#dateformat).
  If no format is specified, then the result will be an empty string.

**Example**

```json
{
  "path": "eventDate",
  "type": "datetime",
  "begin" : "2019-01-01",
  "end": "2019-12-31",
  "format": "%yyyy-%mm-%dd",
}
```

Above example masks the field `eventDate` by returning a random date time
string in the range of January 1st and December 31st in 2019 using a format
like `2019-06-17`.

### Integral Number

This masking type replaces the value of the attribute with a random
integral number. It will replace the value even if it is a string,
Boolean, or `null`.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"integer"`
- `lower` (number, _default: `-100`_): smallest integer value to return
- `upper` (number, _default: `100`_): largest integer value to return

**Example**

```json
{
  "path": "count",
  "type": "integer",
  "lower" : -100,
  "upper": 100
}
```

This masks the field `count` with a random number between
-100 and 100 (inclusive).

### Decimal Number

This masking type replaces the value of the attribute with a random
floating point number. It will replace the value even if it is a string,
Boolean, or `null`.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"decimal"`
- `lower` (number, _default: `-1`_): smallest floating point value to return
- `upper` (number, _default: `1`_): largest floating point value to return
- `scale` (number, _default: `2`_): maximal amount of digits in the
  decimal fraction part

**Examples**

```json
{
  "path": "rating",
  "type": "decimal",
  "lower" : -0.3,
  "upper": 0.3
}
```

This masks the field `rating` with a random floating point number between
-0.3 and +0.3 (inclusive). By default, the decimal has a scale of 2.
That means, it has at most 2 digits after the dot.

The configuration:

```json
{
  "path": "rating",
  "type": "decimal",
  "lower" : -0.3,
  "upper": 0.3,
  "scale": 3
}
```

… will generate numbers with at most 3 decimal digits.

### Credit Card Number

This masking type replaces the value of the attribute with a random
credit card number (as integer number).
See [Luhn algorithm](https://en.wikipedia.org/wiki/Luhn_algorithm)
for details.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"creditCard"`

**Example**

```json
{
  "path": "ccNumber",
  "type": "creditCard"
}
```

This generates a random credit card number to mask field `ccNumber`,
e.g. `4111111414443302`.

### Phone Number

This masking type replaces a phone number with a random one.
It uses the following rule:

- If a character of the original number is a digit
  it will be replaced by a random digit.
- If it is a letter it is replaced by a random letter.
- All other characters are left unchanged.
- If the attribute value is not a string it is replaced by the
  default value.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"phone"`
- `default` (string, _default: `"+1234567890"`_): if the input field
  is not of data type `string`, then this value is used

**Examples**

```json
{
  "path": "phone.landline",
  "type": "phone"
}
```

This will replace an existing phone number with a random one, for instance
`"+31 66-77-88-xx"` might get substituted by `"+75 10-79-52-sb"`.

```json
{
  "path": "phone.landline",
  "type": "phone",
  "default": "+49 12345 123456789"
}
```

This masks a phone number as before, but falls back to a different default
phone number in case the input value is not a string.

### Email Address

This masking type takes an email address, computes a hash value and
splits it into three equal parts `AAAA`, `BBBB`, and `CCCC`. The
resulting email address is in the format `AAAA.BBBB@CCCC.invalid`.
The hash is based on a random secret that is different for each run.

Masking settings:

- `path` (string): which field to mask
- `type` (string): masking function name `"email"`

**Example**

```json
{
  "path": ".email",
  "type": "email"
}
```

This masks every leaf attribute `email` with a random email address
similar to `"EHwG.3AOg@hGU=.invalid"`.
