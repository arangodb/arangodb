Joining together
================

References to other documents
-----------------------------

The character data we imported has an attribute *traits* for each character,
which is an array of strings. It does not store character features directly
however:

```json
{
    "name": "Ned",
    "surname": "Stark",
    "alive": false,
    "age": 41,
    "traits": ["A","H","C","N","P"]
}
```

It is rather a list of letters without an apparent meaning. The idea here is
that *traits* is supposed to store documents keys of another collection, which
we can use to resolve the letters to labels such as "strong". The benefit of
using another collection for the actual traits is, that we can easily query
for all existing traits later on and store labels in multiple languages for
instance in a central place. If we would embed traits directly...

```json
{
    "name": "Ned",
    "surname": "Stark",
    "alive": false,
    "age": 41,
    "traits": [
        {
            "de": "stark",
            "en": "strong"
        },
        {
            "de": "einflussreich",
            "en": "powerful"
        },
        {
            "de": "loyal",
            "en": "loyal"
        },
        {
            "de": "rational",
            "en": "rational"
        },
        {
            "de": "mutig",
            "en": "brave"
        }
    ]
}
```

... it becomes really hard to maintain traits. If you were to rename or
translate one of them, you would need to find all other character documents
with the same trait and perform the changes there too. If we only refer to a
trait in another collection, it is as easy as updating a single document.

<!-- What if Trait doc is deleted? DOCUMENT() skips null -->

![Data model comparison](Comparison_DataModels.png)

Importing traits
----------------

Below you find the traits data. Follow the pattern shown in
[Create documents](CRUD.md#create-documents) to import it:

- Create a document collection *Traits*
- Assign the data to a variable in AQL, `LET data = [ ... ]`
- Use a `FOR` loop to iterate over each array element of the data
- `INSERT` the element `INTO Traits`

![Create Traits collection](Traits_Collection_Creation.png)

```json
[
    { "_key": "A", "en": "strong", "de": "stark" },
    { "_key": "B", "en": "polite", "de": "freundlich" },
    { "_key": "C", "en": "loyal", "de": "loyal" },
    { "_key": "D", "en": "beautiful", "de": "schön" },
    { "_key": "E", "en": "sneaky", "de": "hinterlistig" },
    { "_key": "F", "en": "experienced", "de": "erfahren" },
    { "_key": "G", "en": "corrupt", "de": "korrupt" },
    { "_key": "H", "en": "powerful", "de": "einflussreich" },
    { "_key": "I", "en": "naive", "de": "naiv" },
    { "_key": "J", "en": "unmarried", "de": "unverheiratet" },
    { "_key": "K", "en": "skillful", "de": "geschickt" },
    { "_key": "L", "en": "young", "de": "jung" },
    { "_key": "M", "en": "smart", "de": "klug" },
    { "_key": "N", "en": "rational", "de": "rational" },
    { "_key": "O", "en": "ruthless", "de": "skrupellos" },
    { "_key": "P", "en": "brave", "de": "mutig" },
    { "_key": "Q", "en": "mighty", "de": "mächtig" },
    { "_key": "R", "en": "weak", "de": "schwach" }
]
```

Resolving traits
----------------

Let's start simple by returning only the traits attribute of each character:

```js
FOR c IN Characters
    RETURN c.traits
```

```json
[
    { "traits": ["A","H","C","N","P"] },
    { "traits": ["D","H","C"] },
    ...
]
```

<!-- Obsolete if we add a chapter about attribute manipulation -->
Also see the [Fundamentals of Objects / Documents](../Fundamentals/DataTypes.md#objects--documents)
about attribute access.

We can use the *traits* array together with the `DOCUMENT()` function to use
the elements as document keys and look up them up in the *Traits* collection:

```js
FOR c IN Characters
    RETURN DOCUMENT("Traits", c.traits)
```

```json
[
  [
    {
      "_key": "A",
      "_id": "Traits/A",
      "_rev": "_V5oRUS2---",
      "en": "strong",
      "de": "stark"
    },
    {
      "_key": "H",
      "_id": "Traits/H",
      "_rev": "_V5oRUS6--E",
      "en": "powerful",
      "de": "einflussreich"
    },
    {
      "_key": "C",
      "_id": "Traits/C",
      "_rev": "_V5oRUS6--_",
      "en": "loyal",
      "de": "loyal"
    },
    {
      "_key": "N",
      "_id": "Traits/N",
      "_rev": "_V5oRUT---D",
      "en": "rational",
      "de": "rational"
    },
    {
      "_key": "P",
      "_id": "Traits/P",
      "_rev": "_V5oRUTC---",
      "en": "brave",
      "de": "mutig"
    }
  ],
  [
    {
      "_key": "D",
      "_id": "Traits/D",
      "_rev": "_V5oRUS6--A",
      "en": "beautiful",
      "de": "schön"
    },
    {
      "_key": "H",
      "_id": "Traits/H",
      "_rev": "_V5oRUS6--E",
      "en": "powerful",
      "de": "einflussreich"
    },
    {
      "_key": "C",
      "_id": "Traits/C",
      "_rev": "_V5oRUS6--_",
      "en": "loyal",
      "de": "loyal"
    }
  ],
  ...
]
```

This is a bit too much information, so let's only return English labels using
the [array expansion](../Advanced/ArrayOperators.md#array-expansion) notation:

```js
FOR c IN Characters
    RETURN DOCUMENT("Traits", c.traits)[*].en
```

```json
[
  [
    "strong",
    "powerful",
    "loyal",
    "rational",
    "brave"
  ],
  [
    "beautiful",
    "powerful",
    "loyal"
  ],
  ...
]
```

Merging characters and traits
-----------------------------

Great, we resolved the letters to meaningful traits! But we also need to know
to which character they belong. Thus, we need to merge both the character
document and the data from trait document:

```js
FOR c IN Characters
    RETURN MERGE(c, { traits: DOCUMENT("Traits", c.traits)[*].en } )
```

```json
[
  {
    "_id": "Characters/2861650",
    "_key": "2861650",
    "_rev": "_V1bzsXa---",
    "age": 41,
    "alive": false,
    "name": "Ned",
    "surname": "Stark",
    "traits": [
      "strong",
      "powerful",
      "loyal",
      "rational",
      "brave"
    ]
  },
  {
    "_id": "Characters/2861653",
    "_key": "2861653",
    "_rev": "_V1bzsXa--B",
    "age": 40,
    "alive": false,
    "name": "Catelyn",
    "surname": "Stark",
    "traits": [
      "beautiful",
      "powerful",
      "loyal"
    ]
  },
  ...
]
```

The `MERGE()` functions merges objects together. Because we used an object
`{ traits: ... }` which has the same attribute name *traits* as the original
character attribute, the latter is overwritten by the merge.

<!-- extend later, possibly move to a second article about joins
     (also show how this would be done in relational world with a cross table?)

Join another way
----------------

There is another, more flexible syntax for joins: nested `FOR` loops over
multiple collections, with a `FILTER` condition to match up attributes.
In case of the traits key array, there needs to be a third loop to iterate
over the keys:

`js
FOR c IN Characters
  RETURN MERGE(c, {
    traits: (
      FOR key IN c.traits
        FOR t IN Traits
          FILTER t._key == key
          RETURN t.en
    )
  })
`

The result is identical to the previous example.
-->
