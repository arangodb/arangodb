<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Manipulating the collection

## ArangoOperations.collection

```
ArangoOperations.collection(Class<?> entityClass) : CollectionOperations
```

```
ArangoOperations.collection(String name) : CollectionOperations
```

Returns the operations interface for a collection. If the collection does not exists, it is created automatically.

**Arguments**

- **entityClass**: `Class<?>`

  The entity type representing the collection

- **name**: `String`

  The name of the collection

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);

// -- or --

CollectionOperations collection = template.collection("some-collection-name");
```

## CollectionOperations.truncate

```
CollectionOperations.truncate() : void
```

Removes all documents from the collection, but leaves the indexes intact

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
collection.truncate();
```

## CollectionOperations.drop

```
CollectionOperations.drop() : void
```

Deletes the collection from the database.

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
collection.drop();
```

## CollectionOperations.count

```
CollectionOperation.count() : long
```

Counts the documents in a collection

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
long count = collection.count();
```

## CollectionOperations.getProperties

```
CollectionOperations.getProperties() : CollectionPropertiesEntity
```

Reads the properties of the specified collection

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
CollectionPropertiesEntity properties = collection.getProperties();
```

## CollectionOperation.getIndexes

```
CollectionOperations.getIndexes() : Collection<IndexEntity>
```

Returns all indexes of the collection

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
Collection<IndexEntity> indexes = collection.getIndexes();
```

## CollectionOperations.ensureHashIndex

```
CollectionOperations.ensureHashIndex(Iterable<String> fields, HashIndexOptions options) : IndexEntity
```

Creates a hash index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `HashIndexOptions`

  - **unique**: `Boolean`

    If true, then create a unique index

  - **sparse**: `Boolean`

    If true, then create a sparse index

  - **deduplicate**: `Boolean`

    If false, the deduplication of array values is turned off.

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
IndexEntity index = collection.ensureHashIndex(Arrays.asList("a", "b.c"), new HashIndexOptions());
// the index has been created with the handle `index.getId()`
```

## CollectionOperations.ensureSkiplistIndex

```
CollectionOperations.ensureSkiplistIndex(Iterable<String> fields, SkiplistIndexOptions options) : IndexEntity
```

Creates a skip-list index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `SkiplistIndexOptions`

  - **unique**: `Boolean`

    If true, then create a unique index

  - **sparse**: `Boolean`

    If true, then create a sparse index

  - **deduplicate**: `Boolean`

    If false, the deduplication of array values is turned off.

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
IndexEntity index = collection.ensureSkiplistIndex(Arrays.asList("a", "b.c"), new SkiplistIndexOptions());
// the index has been created with the handle `index.getId()`
```

## CollectionOperations.ensureGeoIndex

```
CollectionOperations.ensureGeoIndex(Iterable<String> fields, GeoIndexOptions options) : IndexEntity
```

Creates a geo index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `GeoIndexOptions`

  - **geoJson**: `Boolean`

    If a geo-spatial index on a location is constructed and geoJson is true, then the order within the array is longitude followed by latitude. This corresponds to the format described in.

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
IndexEntity index = collection.ensureGeoIndex(Arrays.asList("latitude", "longitude"), new GeoIndexOptions());
// the index has been created with the handle `index.getId()`
```

## CollectionOperations.ensureFulltextIndex

```
CollectionOperations.ensureFulltextIndex(Iterable<String> fields, FulltextIndexOptions options) : IndexEntity
```

Creates a fulltext index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `FulltextIndexOptions`

  - **minLength**: `Integer`

    Minimum character length of words to index. Will default to a server-defined value if unspecified. It is thus recommended to set this value explicitly when creating the index.

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
IndexEntity index = collection.ensureFulltextIndex(Arrays.asList("description"), new FulltextIndexOptions());
// the index has been created with the handle `index.getId()`
```

## CollectionOperations.ensurePersistentIndex

```
CollectionOperations.ensurePersistentIndex(Iterable<String> fields, PersistentIndexOptions options) : IndexEntity
```

Creates a persistent index for the collection if it does not already exist.

**Arguments**

- **fields**: `Iterable<String>`

  A list of attribute paths

- **options**: `PersistentIndexOptions`

  - **unique**: `Boolean`

    If true, then create a unique index

  - **sparse**: `Boolean`

    If true, then create a sparse index

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
IndexEntity index = collection.ensurePersistentIndex(Arrays.asList("a", "b.c"), new PersistentIndexOptions());
// the index has been created with the handle `index.getId()`
```

## CollectionOperations.dropIndex

```
CollectionOperations.dropIndex(String id) : void
```

Deletes the index with the given _id_ from the collection.

**Arguments**

- **id**: `String`

  The index-handle

**Examples**

```Java
@Autowired ArangoOperations template;

CollectionOperations collection = template.collection(MyObject.class);
collection.dropIndex("some-index");
```
