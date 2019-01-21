Arangodump Data Maskings
========================

*--maskings path-of-config*

It is possible to mask certain fields during dump. A JSON config file is
used to define which fields should be masked and how.

The general structure of the config file is

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

Using `"*"` has collection name, defines a default behavior for collections not
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
            "type": "xify_front",
            "unmaskedLength": 2
          },
          {
            "path": ".security_id",
            "type": "xify_front",
            "unmaskedLength": 2
          }
        ]
      }
    }

In the example the collection "private" is completely ignored. Only the
structure of the collection "log" is dumped, but not the data itself.
The collection "person" is dumped completely but masking the "name" field if
it occurs on the top-level. It masks the field "security_id" anywhere in the
document. See below for a complete description of the parameters of
"xify_front".

### masking vs. dump-data

*arangodump* also supports a very coarse masking with the option
`--dump-data false`.  This basically removed all data from the dump.

You can either use `--masking` or `--dump-data false`, but not both.

### masking vs. include-collection

*arangodump* also suppers a very coarse masking with the option
`--include-collection`. This will restrict the collections that are
dumped.

It is possible to combine `--masking` and `--include-collection`
this will take the intersection of expertable collections.

Path
----

If the path starts with a `.` then it is considered to be a prefix wildcard
match.  For example, `.name` will match the attribute name `name` all leaf
attributes in the document. Leaf attributes are attributes of type `string`,
`number`, `bool` and `array` (see below). `name` will only match leaf attributes
at top level. `person.name` will match the attribute `name` of a leaf in the
top-level object `person`.

If you have a attribute name that contains a dot, you need to quote the
name with either a tick or a backtick. For example

    "path": "´name.with.dots´"

or

    "path": "`name.with.dots`"

If the attribute value is an array the masking is applied to all the
array elements.

### Example

This following will replace name by a "XXXX" masked string

    {
      "type": "xify_front",
      "path": ".name",
      "unmaskedLength": 2
    }

The object

    {
      "name": "top-level-name",
      "age": 42,
      "nicknames" : [ { "name": "hugo" }, "egon" ],
      "other": {
        "name": [ "emil", { "secret": "superman" } ]
      }
    }

will be replaced as follows:

    {
      "name": "xxxxxxxxxxxxme",
      "age": 42,
      "nicknames" : [ { "name": "xxgo" }, "egon" ],
      "other": {
        "name": [ "xxil", { "secret": "superman" } ]
      }
      }

`egon` and `superman` are not replaced, because they are not in an
attribute value of an attribute `name`.

### sub-objects and arrays

If you specify a path and the attribute value is an array then the
masking decision is applied to each entry of this array as if this
were the value of the attribute.

If the attribute value is a object, then the attribute is not masked.
Instead the sub-object is checked further for leaf attributes.

#### Example

Masking `email` will convert


  { 
    "email" : "email address" 
  }

into

  { 
    "email" : "xxil xxxxxxss" 
  }

because `email` is a leaf sttribute.

  { 
    "email" : [ 
      "address one", 
      "address two" 
    ] 
  } 

will be converted into

  { 
    "email" : [ 
      "xxxxxss xne", 
      "xxxxxss xwo" 
    ] 
  } 

because the array is "unfolded".

  { 
    "email" : { 
      "address" : "email address" 
    } 
    }

will not be changed because `email` is not a leaf attribute.


Masking Functions
-----------------

### random_string

This masking replaces character strings with a random string of the
same length. Other values, e.g. numbers and booleans, are not changed.

    {
      "path": ".name",
      "type": "random_string"
    }

This will replace all values of attributes with key `name` with a
anonymized string. It is not guaranteed that the string will be of
the same length.

A hash of the original string is computed. If the original string is
shorted then the hash will be used. This will result in a longer
string. If the string is longer than hash than the hash characters
will be repeated as many times as needed to reach the full original
string length.

The random string will use the characters `a` to `z` and `A`to `Z`.

    {
      "path": ".name",
      "type": "random_string",
      "characterSet": "abcd01234"
    }

In this case the random string will use the characters `a`, `b`, `c`,
`d` as well as the digits `0`, `1`, `2`, `3`, `4`. If `characterSet`
is empty, it defaults to `a` to `z` and `A` to `Z`.

### xify_front (Enterprise)

This masking replaces characters with `x` and blanks. Alphanumeric characters,
`_` and `-` are replaced by `x`, everything else is replaced by a blank.

    {
      "path": ".name",
      "type": "xify_front",
      "unmaskedLength": 2
    }

This will mask all alphanumeric characters of a word except the last 2.
Words of length 1 and 2 are unmasked. If the attribute value is not a
string the result will be `xxxx`.

    "This is a test!Do you agree?"

will become

    "xxis is a xxst Do xou xxxee "

There is a catch. If you have an index on the attribute the masking
might distort the index efficiency or even cause errors in case of a
unique index.

    {
      "type": "xify_front",
      "path": ".name",
      "unmaskedLength": 2,
      "hash": true
    }

This will add a hash at the end of the string.

    "This is a test!Do you agree?"

will become

    "xxis is a xxst Do xou xxxee  NAATm8c9hVQ="

Note that the hash is based on a random secrect that is different for
each run. This avoids dictionary attacks.

If you need reproducable results, i.e. hash that do not change between
different runs of *arangodump*, you need to specify a secret as seed,
a number which must not be `0`.

    {
      "type": "xify_front",
      "path": ".name",
      "unmaskedLength": 2,
      "hash": true,
      "seed": 246781478647
    }

### zip (Enterprise)

This masking replaces a zip code with a random one. If the attribute value
is not a string it is replaced by `12345`.

    {
      "path": ".code",
      "type": "zip",
    }

This will replace an existing zip with a random one. It uses the following
rule: If a character of the original zip code is a digit it will be replaced
by a random digit. If a character of the original zip code is a letter it
will be replaced by a random letter keeping the case.

    {
      "path": ".code",
      "type": "zip",
      "default": "abcdef"
    }

If the attribute value is not a string use the value of default `abcdef`.

#### Example

If the original zip code is

    50674

it will be replaced by (for example)

    98146

If the original zip code is

    SA34-EA

it will be replaced by (for example)

    OW91-JI

Please note that this will generate random zip. Therefore there is a chance
that it will destroy uniqueness.

### date-time (Enterprise)

This masking replaces the value of the attribute with a random date.

    {
      "type": "date-time",
      "begin" : "2019-01-01",
      "end": "2019-12-31",
      "format": "%yyyy-%mm-%dd",
    }

`begin` and `end` are in ISO8601.

The format is described in
[DATE_FORMAT](https://docs.arangodb.com/3.4/AQL/Functions/Date.html#dateformat).

### integral number (Enterprise)

This masking replaces the value of the attribute with a random integral number.

    {
      "type": "integer",
      "lower" : -100,
      "upper": 100
    }

### decimal (Enterprise)

This masking replaces the value of the attribute with a random decimal.

    {
      "type": "float",
      "lower" : -0.3,
      "upper": 0.3
    }

by default, the decimal has a scale of 2.

    {
      "type": "float",
      "lower" : -0.3,
      "upper": 0.3,
      "scale": 3
    }

will use a scale of 3.

### credit card number (Enterprise)

This masking replaces the value of the attribute with a random credit card number.

    {
      "type": "ccnumber",
    }

See [Luhn](https://en.wikipedia.org/wiki/Luhn_algorithm) for details.

### phone number (Enterprise)

This masking replaces a phone number with a random one. If the attribute value
is not a string it is replaced by `+1234567890`.

    {
      "type": "phone",
      "default": "+4912345123456789"
    }

This will replace an existing phone number with a random one. It uses
the following rule: If a character of the original number is a digit
it will be replaced by a random digit. If it is letter it is replaced
by a letter. All other characters are unchanged.

    {
      "type": "zip",
      "default": "+4912345123456789"
    }

If the attribute value is not a string use the value of default
`+4912345123456789`.

### email address (Enterprise)

This takes the email the email address, computes a hash value and
split this is three equal parts `AAAA`, `BBBB`, and `CCCC`. The
resulting email address is `AAAA.BBBB@CCCC.invalid`.
