Sorting and limiting
====================

Cap the result count
--------------------

It may not always be necessary to return all documents, that a `FOR` loop
would normally return. In those cases, we can limit the amount of documents
with a `LIMIT()` operation:

```js
FOR c IN Characters
    LIMIT 5
    RETURN c.name
```

```json
[
  "Joffrey",
  "Tommen",
  "Tyrion",
  "Roose",
  "Tywin"
]
```

`LIMIT` is followed by a number for the maximum document count. There is a
second syntax however, which allows you to skip a certain amount of record
and return the next *n* documents:

```js
FOR c IN Characters
    LIMIT 2, 5
    RETURN c.name
```

```json
[
  "Tyrion",
  "Roose",
  "Tywin",
  "Samwell",
  "Melisandre"
]
```

See how the second query skipped the first two names and returned the next
five (both results feature Tyrion, Roose and Tywin).

Sort by name
------------

The order in which matching records were returned by the queries shown until
here was basically random. To return them in a defined order, we can add a
`SORT()` operation. It can have a big impact on the result if combined with
a `LIMIT()`, because the result becomes predictable if you sort first.

```js
FOR c IN Characters
    SORT c.name
    LIMIT 10
    RETURN c.name
```

```json
[
  "Arya",
  "Bran",
  "Brienne",
  "Bronn",
  "Catelyn",
  "Cersei",
  "Daario",
  "Daenerys",
  "Davos",
  "Ellaria"
]
```

See how it sorted by name, then returned the ten alphabetically first coming
names. We can reverse the sort order with `DESC` like descending:

```js
FOR c IN Characters
    SORT c.name DESC
    LIMIT 10
    RETURN c.name
```

```json
[
  "Ygritte",
  "Viserys",
  "Varys",
  "Tywin",
  "Tyrion",
  "Tormund",
  "Tommen",
  "Theon",
  "The High Sparrow",
  "Talisa"
]
```

The first sort was ascending, which is the default order. Because it is the
default, it is not required to explicitly ask for `ASC` order.

Sort by multiple attributes
---------------------------

Assume we want to sort by surname. Many of the characters share a surname.
The result order among characters with the same surname is undefined. We can
first sort by surname, then name to determine the order:

```js
FOR c IN Characters
    FILTER c.surname
    SORT c.surname, c.name
    LIMIT 10
    RETURN {
        surname: c.surname,
        name: c.name
    }
```

```json
[
    { "surname": "Baelish", "name": "Petyr" },
    { "surname": "Baratheon", "name": "Joffrey" },
    { "surname": "Baratheon", "name": "Robert" },
    { "surname": "Baratheon", "name": "Stannis" },
    { "surname": "Baratheon", "name": "Tommen" },
    { "surname": "Bolton", "name": "Ramsay" },
    { "surname": "Bolton", "name": "Roose" },
    { "surname": "Clegane", "name": "Sandor" },
    { "surname": "Drogo", "name": "Khal" },
    { "surname": "Giantsbane", "name": "Tormund" }
]
```

Overall, the documents are sorted by last name. If the *surname* is the same
for two characters, the *name* values are compared and the result sorted.

Note that a filter is applied before sorting, to only let documents through,
that actually feature a surname value (many don't have it and would cause
`null` values in the result).

Sort by age
-----------

The order can also be determined by a numeric value, such as the age:

```js
FOR c IN Characters
    FILTER c.age
    SORT c.age
    LIMIT 10
    RETURN {
        name: c.name,
        age: c.age
    }
```

```json
[
    { "name": "Bran", "age": 10 },
    { "name": "Arya", "age": 11 },
    { "name": "Sansa", "age": 13 },
    { "name": "Jon", "age": 16 },
    { "name": "Theon", "age": 16 },
    { "name": "Daenerys", "age": 16 },
    { "name": "Samwell", "age": 17 },
    { "name": "Joffrey", "age": 19 },
    { "name": "Tyrion", "age": 32 },
    { "name": "Brienne", "age": 32 }
]
```

A filter is applied to avoid documents without age attribute. The remaining
documents are sorted by age in ascending order, and the name and age of the
ten youngest characters are returned.

See the [SORT operation](../Operations/Sort.md) and
[LIMIT operation](../Operations/Limit.md) documentation for more details.
