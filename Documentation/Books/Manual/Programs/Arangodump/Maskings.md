Arangodump Data Maskings
========================

*--maskings path-of-config*

It is possible to mask certain fields during dump. A JSON config file is
used to define with fields should be masked and how.

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

Path
----

If the path starts with a `.` then it is considered to be a wildcard match.
For example, `.name` will match the attribute name `name` everywhere in the
document. `name` will only match at top level. `person.name` will match
the attribute `name` in the top-level object `person`.

If you have a attribute name that contains a dot, you need to quote the
name with either a tick or a backtick. For example

    "path": "´name.with.dots´"

or

    "path": "`name.with.dots`"

xify_front
----------

This masking replaces characters with `x` and ` `. Alphanumeric characters,
`_` and `-` are replaced by `x`, everything else is replaced by ` `.

    {
      "path": ".name",
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
different runs of *arangodump*, you need to specify a seed, which must
not be `0`.

    {
      "path": ".name",
      "unmaskedLength": 2,
      "hash": true,
      "seed": 246781478647
    }
