<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulating databases

## create database

```Java
  // create database
  arangoDB.createDatabase("myDatabase");
```

## drop database

```Java
  // drop database
  arangoDB.db("myDatabase").drop();
```

# Manipulating collections

## create collection

```Java
  // create collection
  arangoDB.db("myDatabase").createCollection("myCollection", null);
```

## drop collection

```Java
  // delete collection
  arangoDB.db("myDatabase").collection("myCollection").drop();
```

## truncate collection

```Java
  arangoDB.db("myDatabase").collection("myCollection").truncate();
```
