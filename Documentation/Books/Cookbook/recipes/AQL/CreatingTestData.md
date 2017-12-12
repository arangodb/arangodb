# Creating test data with AQL

## Problem

I want to create some test documents.

## Solution

If you haven't yet created a collection to hold the documents, create one now using the
ArangoShell:

```js
db._create("myCollection");
```

This has created a collection named *myCollection*.

One of the easiest ways to fill a collection with test data is to use an AQL query that
iterates over a range.

Run the following AQL query from the **AQL editor** in the web interface to insert 1,000
documents into the just created collection:

```
FOR i IN 1..1000
  INSERT { name: CONCAT("test", i) } IN myCollection
```

The number of documents to create can be modified easily be adjusting the range boundary
values.

To create more complex test data, adjust the AQL query! 

Let's say we also want a `status` attribute, and fill it with integer values between `1` to 
(including) `5`, with equal distribution. A good way to achieve this is to use the modulo
operator (`%`):

```
FOR i IN 1..1000
  INSERT { 
    name: CONCAT("test", i), 
    status: 1 + (i % 5) 
  } IN myCollection
```

To create pseudo-random values, use the `RAND()` function. It creates pseudo-random numbers
between 0 and 1. Use some factor to scale the random numbers, and `FLOOR()` to convert the
scaled number back to an integer. 

For example, the following query populates the `value` attribute with numbers between 100 and 
150 (including):

```
FOR i IN 1..1000
  INSERT { 
    name: CONCAT("test", i), 
    value: 100 + FLOOR(RAND() * (150 - 100 + 1)) 
  } IN myCollection
```

After the test data has been created, it is often helpful to verify it. The 
`RAND()` function is also a good candidate for retrieving a random sample of the documents in the
collection. This query will retrieve 10 random documents:

```
FOR doc IN myCollection
  SORT RAND()
  LIMIT 10
  RETURN doc
``` 

The `COLLECT` clause is an easy mechanism to run an aggregate analysis on some attribute. Let's
say we wanted to verify the data distribution inside the `status` attribute. In this case we
could run:

```
FOR doc IN myCollection 
  COLLECT value = doc.value WITH COUNT INTO count 
  RETURN { 
    value: value, 
    count: count
  }
```

The above query will provide the number of documents per distinct `value`.

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #aql
