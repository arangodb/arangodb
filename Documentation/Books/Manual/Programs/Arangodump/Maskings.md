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

## MASKING_TYPE

This is a string describing how to mask this collection. Possible values are

- "ignore": the collection is ignored complete and not even the sturcture data
  is dumped.

- "structure": only the collection structure is dumped, but no data at all

- "full": the collection structure and all data is dumped. However, the data
  is subject to maskings defined in the attribute maskinks.

For example:

    {
      "private": {
        "type": "ignore"
      },

      "log": {
        "type": "structure"
      },

      "person": {
        "type": "full",
        "maskings": [
          {
            "path": "name",
            "type": "xify_front",
            "length": 2
          },
          {
            "path": "security_id",
            "type": "xify_front",
            "length": 2
          }
        ]
      }
    }

In the example the collection "private" is completely ignored. Only the
structure of the collection "log" is dump, but not the data itself.
The collection "person" is dumped completed but masking the "name" field when
it occurs on the to-level. It masks the field "security_id" anywhere in the
document.

