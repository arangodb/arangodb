<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Multi Document operations

## insert documents

```Java
  Collection<MyObject> documents = new ArrayList<>;
  documents.add(myObject1);
  documents.add(myObject2);
  documents.add(myObject3);
  arangoDB.db("myDatabase").collection("myCollection").insertDocuments(documents);
```

## delete documents

```Java
  Collection<String> keys = new ArrayList<>;
  keys.add(myObject1.getKey());
  keys.add(myObject2.getKey());
  keys.add(myObject3.getKey());
  arangoDB.db("myDatabase").collection("myCollection").deleteDocuments(keys);
```

## update documents

```Java
  Collection<MyObject> documents = new ArrayList<>;
  documents.add(myObject1);
  documents.add(myObject2);
  documents.add(myObject3);
  arangoDB.db("myDatabase").collection("myCollection").updateDocuments(documents);
```

## replace documents

```Java
  Collection<MyObject> documents = new ArrayList<>;
  documents.add(myObject1);
  documents.add(myObject2);
  documents.add(myObject3);
  arangoDB.db("myDatabase").collection("myCollection").replaceDocuments(documents);
```
