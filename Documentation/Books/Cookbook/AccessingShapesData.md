# Accessing Shapes Data

## Problem
Documents in a collection may have different shapes associated with them. There is no way to query the shapes data directly. So how do you solve this problem?

## Solution
There are two possible ways to do this. 

*A) The fast way with some random samplings:*

1. Ask for a random document (`db.<collection>.any()`) and note its top-level attribute names
2. Repeat this for at least 10 times. After that repeat it only if you think it's worth it. 

Following is an example of an implementation:

```js
attributes(db.myCollection);


function attributes(collection) {
  "use strict"

  var probes = 10;
  var maxRounds = 3;
  var threshold = 0.5;

  var maxDocuments = collection.count();

  if (maxDocuments < probes) {
    probes = maxDocuments;
  }

  if (probes === 0) {
    return [ ];
  }

  var attributes = { };

  while (maxRounds--) {
    var newDocuments = 0;
    var n = probes;
    while (n--) {
      var doc = collection.any();
      var found = false;
      var keys = Object.keys(doc);

    for (var i = 0; i < keys.length; ++i) {
      if (attributes.hasOwnProperty(keys[i])) {
        ++attributes[keys[i]];
      }
      else {
        attributes[keys[i]] = 1;
        found = true;
      }
    }

     if (found) {
      ++newDocuments;
     }
    }

    if (newDocuments / probes <= threshold) {
      break;
    }
  }

  return Object.keys(attributes); 
} 
```

*B) The way to find all top-level attributes*

If you don't mind to make some extra inserts and you don't care about deletion or updates of documents you can use the following:

```js
db._create("mykeys");
db.mykeys.ensureUniqueSkiplist("attribute");


function insert(collection, document) {
  var result = collection.save(document);

  try { 
    var keys = Objects.keys(document);

    for (i = 0; i < keys.length; ++i) {
      try {
        db.mykeys.save({ attribute: keys[i] });
      } 
        catch (err1) {
        // potential unique key constraint violations
        }
    } 
  }
  catch (err2) {
  }

  return result;
}
```

## Comment

*A) The fast way with some random samplings:*

You get some random sampling with bounded complexity. 
If you have a variety of attributes you should repeat the procedure more than 10 times.

The procedure can be implemented as a server side action. 

*B) The way to find all top-level attributes*:

This procedure will not care about updates or deletions of documents.
Also only the top-level attribute of the documents will be inserted and nested one ignored. 

The procedure can be implemented as a server side action. 

**Author:** [Arangodb](https://github.com/arangodb)

**Tags:** #collection #database