<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# AQL

## Executing an AQL statement

Every AQL operations works with POJOs (e.g. MyObject), VelocyPack (VPackSlice) and Json (String).

E.g. get all Simpsons aged 3 or older in ascending order:

```Java
  arangoDB.createDatabase("myDatabase");
  ArangoDatabase db = arangoDB.db("myDatabase");

  db.createCollection("myCollection");
  ArangoCollection collection = db.collection("myCollection");

  collection.insertDocument(new MyObject("Homer", 38));
  collection.insertDocument(new MyObject("Marge", 36));
  collection.insertDocument(new MyObject("Bart", 10));
  collection.insertDocument(new MyObject("Lisa", 8));
  collection.insertDocument(new MyObject("Maggie", 2));

  Map<String, Object> bindVars = new HashMap<>();
  bindVars.put("age", 3);

  ArangoCursor<MyObject> cursor = db.query(query, bindVars, null, MyObject.class);

  for(; cursor.hasNext;) {
    MyObject obj = cursor.next();
    System.out.println(obj.getName());
  }
```

or return the AQL result as VelocyPack:

```Java
  ArangoCursor<VPackSlice> cursor = db.query(query, bindVars, null, VPackSlice.class);

  for(; cursor.hasNext;) {
    VPackSlice obj = cursor.next();
    System.out.println(obj.get("name").getAsString());
  }
```

**Note**: The parameter `type` in `query()` has to match the result of the query, otherwise you get an VPackParserException. E.g. you set `type` to `BaseDocument` or a POJO and the query result is an array or simple type, you get an VPackParserException caused by VPackValueTypeException: Expecting type OBJECT.
