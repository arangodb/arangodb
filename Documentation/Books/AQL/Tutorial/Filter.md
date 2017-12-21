Matching documents
==================

So far, we either looked up a single document, or returned the entire character
collection. For the lookup, we used the `DOCUMENT()` function, which means we
can only find documents by their key or ID.

To find documents that fulfill certain criteria more complex than key equality,
there is the `FILTER` operation in AQL, which enables us to formulate arbitrary
conditions for documents to match.

Equality condition
------------------

```js
FOR c IN Characters
    FILTER c.name == "Ned"
    RETURN c
```

The filter condition reads like: "the attribute *name* of a character document
must be equal to the string *Ned*". If the condition applies, character
document gets returned. This works with any attribute likewise:

```js
FOR c IN Characters
    FILTER c.surname == "Stark"
    RETURN c
```

Range conditions
----------------

Strict equality is one possible condition we can state. There are plenty of
other conditions we can formulate however. For example, we could ask for all
young characters:

```js
FOR c IN Characters
    FILTER c.age >= 13
    RETURN c.name
```

```json
[
  "Joffrey",
  "Tyrion",
  "Samwell",
  "Ned",
  "Catelyn",
  "Cersei",
  "Jon",
  "Sansa",
  "Brienne",
  "Theon",
  "Davos",
  "Jaime",
  "Daenerys"
]
```

The operator `>=` stands for *greater-or-equal*, so every character of age 13
or older is returned (only their name in the example). We can return names
and age of all characters younger than 13 by changing the operator to
*less-than* and using the object syntax to define a subset of attributes to
return:

```js
FOR c IN Characters
    FILTER c.age < 13
    RETURN { name: c.name, age: c.age }
```

```json
[
  { "name": "Tommen", "age": null },
  { "name": "Arya", "age": 11 },
  { "name": "Roose", "age": null },
  ...
]
```

You may notice that it returns name and age of 30 characters, most with an
age of `null`. The reason for this is, that `null` is the fallback value if
an attribute is requested by the query, but no such attribute exists in the
document, and the `null` is compares to numbers as lower (see
[Type and value order](../Fundamentals/TypeValueOrder.md)). Hence, it
accidentally fulfills the age criterion `c.age < 13` (`null < 13`).

Multiple conditions
-------------------

To not let documents pass the filter without an age attribute, we can add a
second criterion:

```js
FOR c IN Characters
    FILTER c.age < 13
    FILTER c.age != null
    RETURN { name: c.name, age: c.age }
```

```json
[
  { "name": "Arya", "age": 11 },
  { "name": "Bran", "age": 10 }
]
```

This could equally be written with a boolean `AND` operator as:

```js
FOR c IN Characters
    FILTER c.age < 13 AND c.age != null
    RETURN { name: c.name, age: c.age }
```

And the second condition could as well be `c.age > null`.

Alternative conditions
----------------------

If you want documents to fulfill one or another condition, possibly for
different attributes as well, use `OR`:

```js
FOR c IN Characters
    FILTER c.name == "Jon" OR c.name == "Joffrey"
    RETURN { name: c.name, surname: c.surname }
```

```json
[
  { "name": "Joffrey", "surname": "Baratheon" },
  { "name": "Jon", "surname": "Snow" }
]
```

See more details about [Filter operations](../Operations/Filter.md).
