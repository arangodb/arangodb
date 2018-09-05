<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# ArangoSearch API

These functions implement the
[HTTP API for ArangoSearch Views](../../../..//HTTP/Views/ArangoSearch.html).

## ArangoDatabase.createArangoSearch

```
ArangoDatabase.createArangoSearch(String name, ArangoSearchCreateOptions options) : ViewEntity
```

Creates a ArangoSearch View with the given _options_, then returns view information from the server.

**Arguments**

- **name**: `String`

  The name of the view

- **options**: `ArangoSearchCreateOptions`

  - **locale**: `String`

    The default locale used for queries on analyzed string values (default: C).

  - **commitIntervalMsec**: `Long`

    Wait at least this many milliseconds between committing index data changes and making them visible to queries (default: 60000, to disable use: 0). For the case where there are a lot of inserts/updates, a lower value, until commit, will cause the index not to account for them and memory usage would continue to grow. For the case where there are a few inserts/updates, a higher value will impact performance and waste disk space for each commit call without any added benefits.

  - **cleanupIntervalStep**: `Long`

    Wait at least this many commits between removing unused files in data directory (default: 10, to disable use: 0). For the case where the consolidation policies merge segments often (i.e. a lot of commit+consolidate), a lower value will cause a lot of disk space to be wasted. For the case where the consolidation policies rarely merge segments (i.e. few inserts/deletes), a higher value will impact performance without any added benefits.

  - **threshold**: `ConsolidateThreshold[]`

    A list of consolidate thresholds

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
db.createArangoSearch("potatoes", new ArangoSearchPropertiesOptions());
// the ArangoSearch View "potatoes" now exists
```

## ArangoSearch.create

```
ArangoSearch.create(ArangoSearchCreateOptions options) : ViewEntity
```

Creates a ArangoSearch View with the given _options_, then returns view information from the server.

Alternative for [ArangoDatabase.createArangoSearch](#arangodatabasecreatearangosearch).

**Arguments**

- **options**: `ArangoSearchCreateOptions`

  - **locale**: `String`

    The default locale used for queries on analyzed string values (default: C).

  - **commitIntervalMsec**: `Long`

    Wait at least this many milliseconds between committing index data changes and making them visible to queries (default: 60000, to disable use: 0). For the case where there are a lot of inserts/updates, a lower value, until commit, will cause the index not to account for them and memory usage would continue to grow. For the case where there are a few inserts/updates, a higher value will impact performance and waste disk space for each commit call without any added benefits.

  - **cleanupIntervalStep**: `Long`

    Wait at least this many commits between removing unused files in data directory (default: 10, to disable use: 0). For the case where the consolidation policies merge segments often (i.e. a lot of commit+consolidate), a lower value will cause a lot of disk space to be wasted. For the case where the consolidation policies rarely merge segments (i.e. few inserts/deletes), a higher value will impact performance without any added benefits.

  - **threshold**: `ConsolidateThreshold[]`

    A list of consolidate thresholds

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoSearch view = db.arangoSearch("potatoes");

view.create(new ArangoSearchPropertiesOptions());
// the ArangoSearch View "potatoes" now exists
```

## ArangoSearch.getProperties

```
ArangoSearch.getProperties() : ArangoSearchPropertiesEntity
```

Reads the properties of the specified view.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoSearch view = db.arangoSearch("potatoes");

ArangoSearchPropertiesEntity properties = view.getProperties();
```

## ArangoSearch.updateProperties

```
ArangoSearch.updateProperties(ArangoSearchPropertiesOptions options) : ArangoSearchPropertiesEntity
```

Partially changes properties of the view.

**Arguments**

- **options**: `ArangoSearchPropertiesOptions`

  - **locale**: `String`

    The default locale used for queries on analyzed string values (default: C).

  - **commitIntervalMsec**: `Long`

    Wait at least this many milliseconds between committing index data changes and making them visible to queries (default: 60000, to disable use: 0). For the case where there are a lot of inserts/updates, a lower value, until commit, will cause the index not to account for them and memory usage would continue to grow. For the case where there are a few inserts/updates, a higher value will impact performance and waste disk space for each commit call without any added benefits.

  - **cleanupIntervalStep**: `Long`

    Wait at least this many commits between removing unused files in data directory (default: 10, to disable use: 0). For the case where the consolidation policies merge segments often (i.e. a lot of commit+consolidate), a lower value will cause a lot of disk space to be wasted. For the case where the consolidation policies rarely merge segments (i.e. few inserts/deletes), a higher value will impact performance without any added benefits.

  - **threshold**: `ConsolidateThreshold[]`

    A list of consolidate thresholds

  - **link**: `CollectionLink[]`

    A list of linked collections

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoSearch view = db.arangoSearch("some-view");

view.updateProperties(new ArangoSearchPropertiesOptions().link(CollectionLink.on("myCollection").fields(FieldLink.on("value").analyzers("identity"))));
```

## ArangoSearch.replaceProperties

```
ArangoSearch.replaceProperties(ArangoSearchPropertiesOptions options) : ArangoSearchPropertiesEntity
```

Changes properties of the view.

**Arguments**

- **options**: `ArangoSearchPropertiesOptions`

  - **locale**: `String`

    The default locale used for queries on analyzed string values (default: C).

  - **commitIntervalMsec**: `Long`

    Wait at least this many milliseconds between committing index data changes and making them visible to queries (default: 60000, to disable use: 0). For the case where there are a lot of inserts/updates, a lower value, until commit, will cause the index not to account for them and memory usage would continue to grow. For the case where there are a few inserts/updates, a higher value will impact performance and waste disk space for each commit call without any added benefits.

  - **cleanupIntervalStep**: `Long`

    Wait at least this many commits between removing unused files in data directory (default: 10, to disable use: 0). For the case where the consolidation policies merge segments often (i.e. a lot of commit+consolidate), a lower value will cause a lot of disk space to be wasted. For the case where the consolidation policies rarely merge segments (i.e. few inserts/deletes), a higher value will impact performance without any added benefits.

  - **threshold**: `ConsolidateThreshold[]`

    A list of consolidate thresholds

  - **link**: `CollectionLink[]`

    A list of linked collections

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoSearch view = db.arangoSearch("some-view");

view.replaceProperties(new ArangoSearchPropertiesOptions().link(CollectionLink.on("myCollection").fields(FieldLink.on("value").analyzers("identity"))));
```
