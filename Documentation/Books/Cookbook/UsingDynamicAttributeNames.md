# Using dynamic attribute names in AQL

## Problem

I want an AQL query to return results with attribute names assembled by a function,
or with a variable number of attributes.

This will not work by specifying the result using a regular object literal, as object
literals require the names and numbers of attributes to be fixed at query compile time.

## Solution

There are several solutions to getting dynamic attribute names to work.

### Subquery solution

A general solution is to let a subquery or another function to produce the dynamic
attribute names, and finally pass them through the `ZIP()` function to create an object
from them.

Let's assume we want to process the following input documents:

```json
{ "name" : "test", "gender" : "f", "status" : "active", "type" : "user" }
{ "name" : "dummy", "gender" : "m", "status" : "inactive", "type" : "unknown", "magicFlag" : 23 }
```

Let's also assume our goal for each of these documents is to return only the attribute
names that contain the letter `a`, together with their respective values.

To extract the attribute names and values from the original documents, we can use a subquery
as follows:

```
LET documents = [
  { "name" : "test"," gender" : "f", "status" : "active", "type" : "user" },
  { "name" : "dummy", "gender" : "m", "status" : "inactive", "type" : "unknown", "magicFlag" : 23 }
]

FOR doc IN documents
  RETURN (
    FOR name IN ATTRIBUTES(doc)
      FILTER LIKE(name, '%a%')
      RETURN {
        name: name,
        value: doc[name]
      }
  )
```

The subquery will only let attribute names pass that contain the letter `a`. The results
of the subquery are then made available to the main query and will be returned. But the
attribute names in the result are still `name` and `value`, so we're not there yet.

So let's also employ AQL's `ZIP()` function, which can create an object from two arrays:

* the first parameter to `ZIP()` is an array with the attribute names
* the second parameter to `ZIP()` is an array with the attribute values

Instead of directly returning the subquery result, we first capture it in a variable, and
pass the variable's `name` and `value` components into `ZIP()` like this:

```
LET documents = [
  { "name" : "test"," gender" : "f", "status" : "active", "type" : "user" },
  { "name" : "dummy", "gender" : "m", "status" : "inactive", "type" : "unknown", "magicFlag" : 23 }
]

FOR doc IN documents
  LET attributes = (
    FOR name IN ATTRIBUTES(doc)
      FILTER LIKE(name, '%a%')
      RETURN {
        name: name,
        value: doc[name]
      }
  )
  RETURN ZIP(attributes[*].name, attributes[*].value)
```

Note that we have to use the expansion operator (`[*]`) on `attributes` because `attributes`
itself is an array, and we want either the `name` attribute or the `value` attribute of each
of its members.

To prove this is working, here is the above query's result:

```json
[
  {
    "name": "test",
    "status": "active"
  },
  {
    "name": "dummy",
    "status": "inactive",
    "magicFlag": 23
  }
]
```

As can be seen, the two results have a different amount of result attributes. We can also
make the result a bit more dynamic by prefixing each attribute with the value of the `name`
attribute:

```
LET documents = [
  { "name" : "test"," gender" : "f", "status" : "active", "type" : "user" },
  { "name" : "dummy", "gender" : "m", "status" : "inactive", "type" : "unknown", "magicFlag" : 23 }
]

FOR doc IN documents
  LET attributes = (
    FOR name IN ATTRIBUTES(doc)
      FILTER LIKE(name, '%a%')
      RETURN {
        name: CONCAT(doc.name, '-', name),
        value: doc[name]
      }
  )
  RETURN ZIP(attributes[*].name, attributes[*].value)
```

That will give us document-specific attribute names like this:

```json
[
  {
    "test-name": "test",
    "test-status": "active"
  },
  {
    "dummy-name": "dummy",
    "dummy-status": "inactive",
    "dummy-magicFlag": 23
  }
]
```

### Using expressions as attribute names (ArangoDB 2.5)

If the number of dynamic attributes to return is known in advance, and only the attribute names
need to be calculated using an expression, then there is another solution. 

ArangoDB 2.5 and higher allow using expressions instead of fixed attribute names in object literals.
Using expressions as attribute names requires enclosing the expression in extra `[` and `]` to
disambiguate them from regular, unquoted attribute names.

Let's create a result that returns the original document data contained in a dynamically named
attribute. We'll be using the expression `doc.type` for the attribute name. We'll also return
some other attributes from the original documents, but prefix them with the documents' `_key` 
attribute values. For this we also need attribute name expressions.

Here is a query showing how to do this. The attribute name expressions all required to be 
enclosed in `[` and `]` in order to make this work:

```
LET documents = [
  { "_key" : "3231748397810", "gender" : "f", "status" : "active", "type" : "user" },
  { "_key" : "3231754427122", "gender" : "m", "status" : "inactive", "type" : "unknown" }
]

FOR doc IN documents
  RETURN {
    [ doc.type ] : {
      [ CONCAT(doc._key, "_gender") ] : doc.gender,
      [ CONCAT(doc._key, "_status") ] : doc.status
    }
  }
```

This will return:

```json
[
  {
    "user": {
      "3231748397810_gender": "f",
      "3231748397810_status": "active"
    }
  },
  {
    "unknown": {
      "3231754427122_gender": "m",
      "3231754427122_status": "inactive"
    }
  }
]
```

Note: attribute name expressions and regular, unquoted attribute names can be mixed.

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #aql
