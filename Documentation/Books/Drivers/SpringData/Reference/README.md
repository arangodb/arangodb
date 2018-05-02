<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Spring Data ArangoDB - Reference

# Template

With `ArangoTemplate` Spring Data ArangoDB offers a central support for interactions with the database over a rich feature set. It mostly offers the features from the ArangoDB Java driver with additional exception translation from the drivers exceptions to the Spring Data access exceptions inheriting the `DataAccessException` class.
The `ArangoTemplate` class is the default implementation of the operations interface `ArangoOperations` which developers of Spring Data are encouraged to code against.

# Repositories

## Introduction

Spring Data Commons provides a composable repository infrastructure which Spring Data ArangoDB is built on. These allow for interface-based composition of repositories consisting of provided default implementations for certain interfaces (like `CrudRepository`) and custom implementations for other methods.

## Instantiating

Instances of a Repository are created in Spring beans through the auto-wired mechanism of Spring.

```java
public class MySpringBean {

  @Autowired
  private MyRepository rep;

}
```

## Return types

The method return type for single results can be a primitive type, a domain class, `Map<String, Object>`, `BaseDocument`, `BaseEdgeDocument`, `Optional<Type>`, `GeoResult<Type>`.
The method return type for multiple results can additionally be `ArangoCursor<Type>`, `Iterable<Type>`, `Collection<Type>`, `List<Type>`, `Set<Type>`, `Page<Type>`, `Slice<Type>`, `GeoPage<Type>`, `GeoResults<Type>` where Type can be everything a single result can be.

## Query methods

Queries using [ArangoDB Query Language (AQL)](https://docs.arangodb.com/current/AQL/index.html) can be supplied with the `@Query` annotation on methods. `AqlQueryOptions` can also be passed to the driver, as an argument anywhere in the method signature.

There are three ways of passing bind parameters to the query in the query annotation.

Using number matching, arguments will be substituted into the query in the order they are passed to the query method.

```java
public interface MyRepository extends Repository<Customer, String>{

  @Query("FOR c IN customers FILTER c.name == @0 AND c.surname == @2 RETURN c")
  ArangoCursor<Customer> query(String name, AqlQueryOptions options, String surname);

}
```

With the `@Param` annotation, the argument will be placed in the query at the place corresponding to the value passed to the `@Param` annotation.

```java
public interface MyRepository extends Repository<Customer, String>{

  @Query("FOR c IN customers FILTER c.name == @name AND c.surname == @surname RETURN c")
  ArangoCursor<Customer> query(@Param("name") String name, @Param("surname") String surname);

}
```

 In addition you can use a parameter of type `Map<String, Object>` annotated with `@BindVars` as your bind parameters. You can then fill the map with any parameter used in the query. (see [here](https://docs.arangodb.com/3.1/AQL/Fundamentals/BindParameters.html#bind-parameters) for more Information about Bind Parameters).

```java
public interface MyRepository extends Repository<Customer, String>{

  @Query("FOR c IN customers FILTER c.name == @name AND c.surname = @surname RETURN c")
  ArangoCursor<Customer> query(@BindVars Map<String, Object> bindVars);

}
```

A mixture of any of these methods can be used. Parameters with the same name from an `@Param` annotation will override those in the `bindVars`.

```java
public interface MyRepository extends Repository<Customer, String>{

  @Query("FOR c IN customers FILTER c.name == @name AND c.surname = @surname RETURN c")
  ArangoCursor<Customer> query(@BindVars Map<String, Object> bindVars, @Param("name") String name);

}
```

## Derived queries

Spring Data ArangoDB supports queries derived from methods names by splitting it into its semantic parts and converting into AQL. The mechanism strips the prefixes `find..By`, `get..By`, `query..By`, `read..By`, `stream..By`, `count..By`, `exists..By`, `delete..By`, `remove..By` from the method and parses the rest. The By acts as a separator to indicate the start of the criteria for the query to be built. You can define conditions on entity properties and concatenate them with `And` and `Or`.

The complete list of part types for derived methods is below, where doc is a document in the database

Keyword | Sample | Predicate
----------|----------------|--------
IsGreaterThan, GreaterThan, After | findByAgeGreaterThan(int age) | doc.age > age
IsGreaterThanEqual, GreaterThanEqual | findByAgeIsGreaterThanEqual(int age) | doc.age >= age
IsLessThan, LessThan, Before | findByAgeIsLessThan(int age) | doc.age < age
IsLessThanEqualLessThanEqual | findByAgeLessThanEqual(int age) | doc.age <= age
IsBetween, Between | findByAgeBetween(int lower, int upper) | lower < doc.age < upper
IsNotNull, NotNull | findByNameNotNull() | doc.name != null
IsNull, Null | findByNameNull() | doc.name == null
IsLike, Like | findByNameLike(String name) | doc.name LIKE name
IsNotLike, NotLike | findByNameNotLike(String name) | NOT(doc.name LIKE name)
IsStartingWith, StartingWith, StartsWith | findByNameStartsWith(String prefix) | doc.name LIKE prefix
IsEndingWith, EndingWith, EndsWith | findByNameEndingWith(String suffix) | doc.name LIKE suffix
Regex, MatchesRegex, Matches | findByNameRegex(String pattern) | REGEX_TEST(doc.name, name, ignoreCase)
(No Keyword) | findByFirstName(String name) | doc.name == name
IsTrue, True | findByActiveTrue() | doc.active == true
IsFalse, False | findByActiveFalse() | doc.active == false
Is, Equals | findByAgeEquals(int age) | doc.age == age
IsNot, Not | findByAgeNot(int age) | doc.age != age
IsIn, In | findByNameIn(String[] names) | doc.name IN names
IsNotIn, NotIn | findByNameIsNotIn(String[] names) | doc.name NOT IN names
IsContaining, Containing, Contains | findByFriendsContaining(String name) | name IN doc.friends
IsNotContaining, NotContaining, NotContains | findByFriendsNotContains(String name) | name NOT IN doc.friends
Exists | findByFriendNameExists() | HAS(doc.friend, name)


```java
public interface MyRepository extends Repository<Customer, String> {

  // FOR c IN customers FILTER c.name == @0 RETURN c
  ArangoCursor<Customer> findByName(String name);
  ArangoCursor<Customer> getByName(String name);

  // FOR c IN customers
  // FILTER c.name == @0 && c.age == @1
  // RETURN c
  ArangoCursor<Customer> findByNameAndAge(String name, int age);

  // FOR c IN customers
  // FILTER c.name == @0 || c.age == @1
  // RETURN c
  ArangoCursor<Customer> findByNameOrAge(String name, int age);
}
```

You can apply sorting for one or multiple sort criteria by appending `OrderBy` to the method and `Asc` or `Desc` for the directions.

```java
public interface MyRepository extends Repository<Customer, String> {

  // FOR c IN customers
  // FITLER c.name == @0
  // SORT c.age DESC RETURN c
  ArangoCursor<Customer> getByNameOrderByAgeDesc(String name);

  // FOR c IN customers
  // FILTER c.name = @0
  // SORT c.name ASC, c.age DESC RETURN c
  ArangoCursor<Customer> findByNameOrderByNameAscAgeDesc(String name);

}
```

### Geospatial queries

Geospatial queries are a subsection of derived queries. To use a geospatial query on a collection, a geo index must exist on that collection. A geo index can be created on a field which is a two element array, corresponding to latitude and longitude coordinates.

As a subsection of derived queries, geospatial queries support all the same return types, but also support the three return types `GeoPage, GeoResult and Georesults`. These types must be used in order to get the distance of each document as generated by the query.

There are two kinds of geospatial query, Near and Within. Near sorts  documents by distance from the given point, while within both sorts and filters documents, returning those within the given distance range or shape.

```java
public interface MyRepository extends Repository<City, String> {

    GeoResult<City> getByLocationNear(Point point);

    GeoResults<City> findByLocationWithinOrLocationWithin(Box box, Polygon polygon);

    //Equivalent queries
    GeoResults<City> findByLocationWithinOrLocationWithin(Point point, int distance);
    GeoResults<City> findByLocationWithinOrLocationWithin(Point point, Distance distance);
    GeoResults<City> findByLocationWithinOrLocationWithin(Circle circle);

}
```

## Property expression

Property expressions can refer only to direct and nested properties of the managed domain class. The algorithm checks the domain class for the entire expression as the property. If the check fails, the algorithm splits up the expression at the camel case parts from the right and tries to find the corresponding property.

```java
@Document("customers")
public class Customer {
  private Address address;
}

public class Address {
  private ZipCode zipCode;
}

public interface MyRepository extends Repository<Customer, String> {

  // 1. step: search domain class for a property "addressZipCode"
  // 2. step: search domain class for "addressZip.code"
  // 3. step: search domain class for "address.zipCode"
  ArangoCursor<Customer> findByAddressZipCode(ZipCode zipCode);
}
```

It is possible for the algorithm to select the wrong property if the domain class also has a property which matches the first split of the expression. To resolve this ambiguity you can use _ as a separator inside your method-name to define traversal points.

```java
@Document("customers")
public class Customer {
  private Address address;
  private AddressZip addressZip;
}

public class Address {
  private ZipCode zipCode;
}

public class AddressZip {
  private String code;
}

public interface MyRepository extends Repository<Customer, String> {

  // 1. step: search domain class for a property "addressZipCode"
  // 2. step: search domain class for "addressZip.code"
  // creates query with "x.addressZip.code"
  ArangoCursor<Customer> findByAddressZipCode(ZipCode zipCode);

  // 1. step: search domain class for a property "addressZipCode"
  // 2. step: search domain class for "addressZip.code"
  // 3. step: search domain class for "address.zipCode"
  // creates query with "x.address.zipCode"
  ArangoCursor<Customer> findByAddress_ZipCode(ZipCode zipCode);

}
```

## Special parameter handling

### Bind parameters

AQL supports the usage of [bind parameters](https://docs.arangodb.com/3.1/AQL/Fundamentals/BindParameters.html) which you can define with a method parameter named `bindVars` of type `Map<String, Object>`.

```java
public interface MyRepository extends Repository<Customer, String> {

  @Query("FOR c IN customers FILTER c[@field] == @value RETURN c")
  ArangoCursor<Customer> query(Map<String, Object> bindVars);

}

Map<String, Object> bindVars = new HashMap<String, Object>();
bindVars.put("field", "name");
bindVars.put("value", "john";

// will execute query "FOR c IN customers FILTER c.name == "john" RETURN c"
ArangoCursor<Customer> cursor = myRepo.query(bindVars);
```

### AQL query options

You can set additional options for the query and the created cursor over the class `AqlQueryOptions` which you can simply define as a method parameter without a specific name. AqlQuery options can also be defined with the `@QueryOptions` annotation, as shown below. AqlQueryOptions from an annotation and those from an argument are merged if both exist, with those in the argument taking precedence.

The `AqlQueryOptions` allows you to set the cursor time-to-life, batch-size, caching flag and several other settings. This special parameter works with both query-methods and finder-methods. Keep in mind that some options, like time-to-life, are only effective if the method return type is`ArangoCursor<T>` or `Iterable<T>`.

```java
public interface MyRepository extends Repository<Customer, String> {


  @Query("FOR c IN customers FILTER c.name == @0 RETURN c")
  Iterable<Customer> query(String name, AqlQueryOptions options);


  Iterable<Customer> findByName(String name, AqlQueryOptions options);


  @QueryOptions(maxPlans = 1000, ttl = 128)
  ArangoCursor<Customer> findByAddressZipCode(ZipCode zipCode);


  @Query("FOR c IN customers FILTER c[@field] == @value RETURN c")
  @QueryOptions(cache = true, ttl = 128)
  ArangoCursor<Customer> query(Map<String, Object> bindVars, AqlQueryOptions options);

}
```

# Mapping

## Introduction

In this section we will describe the features and conventions for mapping Java objects to documents and how to override those conventions with annotation based mapping metadata.

## Conventions

* The Java class name is mapped to the collection name
* The non-static fields of a Java object are used as fields in the stored document
* The Java field name is mapped to the stored document field name
* All nested Java object are stored as nested objects in the stored document
* The Java class needs a constructor which meets the following criteria:
  * in case of a single constructor:
    * a non-parameterized constructor or
    * a parameterized constructor
  * in case of multiple constructors:
    * a non-parameterized constructor or
    * a parameterized constructor annotated with `@PersistenceConstructor`

## Type conventions

ArangoDB uses [VelocyPack](https://github.com/arangodb/velocypack) as it's internal storage format which supports a large number of data types. In addition Spring Data ArangoDB offers - with the underlying Java driver - built-in converters to add additional types to the mapping.

Java type | VelocyPack type
----------|----------------
java.lang.String | string
java.lang.Boolean | bool
java.lang.Integer | signed int 4 bytes, smallint
java.lang.Long | signed int 8 bytes, smallint
java.lang.Short | signed int 2 bytes, smallint
java.lang.Double | double
java.lang.Float | double
java.math.BigInteger | signed int 8 bytes, unsigned int 8 bytes
java.math.BigDecimal | double
java.lang.Number | double
java.lang.Character | string
java.util.Date | string (date-format ISO 8601)
java.sql.Date | string (date-format ISO 8601)
java.sql.Timestamp | string (date-format ISO 8601)
java.util.UUID | string
java.lang.byte[] | string (Base64)

## Type mapping
As collections in ArangoDB can contain documents of various types, a mechanism to retrieve the correct Java class is required. The type information of properties declared in a class may not be enough to restore the original class (due to inheritance). If the declared complex type and the actual type do not match, information about the actual type is stored together with the document. This is necessary to restore the correct type when reading from the DB. Consider the following example:

```java
public class Person {
    private String name;
    private Address homeAddress;
    // ...
	
    // getters and setters omitted
}

public class Employee extends Person {
    private Address workAddress;
    // ...

    // getters and setters omitted
}

public class Address {
    private final String street;
    private final String number;
    // ...

    public Address(String street, String number) {
        this.street = street;
        this.number = number;
    }

    // getters omitted
}

@Document
public class Company {
    @Key 
    private String key;
    private Person manager;

    // getters and setters omitted
}

Employee manager = new Employee();
manager.setName("Jane Roberts");
manager.setHomeAddress(new Address("Park Avenue", "432/64"));
manager.setWorkAddress(new Address("Main Street",  "223"));
Company comp = new Company();
comp.setManager(manager);
```

The serialized document for the DB looks like this:

```json
{
  "manager": {
    "name": "Jane Roberts",
    "homeAddress": {
        "street": "Park Avenue",
        "number": "432/64"
    },
    "workAddress": {
        "street": "Main Street",
        "number": "223"
    },
    "_class": "com.arangodb.Employee"
  },
  "_class": "com.arangodb.Company"
}
```

Type hints are written for top-level documents (as a collection can contain different document types) as well as for every value if it's a complex type and a sub-type of the property type declared. `Map`s and `Collection`s are excluded from type mapping. Without the additional information about the concrete classes used, the document couldn't be restored in Java. The type information of the `manager` property is not enough to determine the `Employee` type. The `homeAddress` and `workAddress` properties have the same actual and defined type, thus no type hint is needed.

### Customizing type mapping
By default, the fully qualified class name is stored in the documents as a type hint. A custom type hint can be set with the `@TypeAlias("my-alias")` annotation on an entity. Make sure that it is an unique identifier across all entities. If we would add a `TypeAlias("employee")` annotation to the `Employee` class above, it would be persisted as `"_class": "employee"`.

The default type key is `_class` and can be changed by overriding the `typeKey()` method of the `AbstractArangoConfiguration` class.

If you need to further customize the type mapping process, the `arangoTypeMapper()` method of the configuration class can be overridden. The included `DefaultArangoTypeMapper` can be customized by providing a list of [`TypeInformationMapper`](https://docs.spring.io/spring-data/commons/docs/current/api/org/springframework/data/convert/TypeInformationMapper.html)s that create aliases from types and vice versa.

In order to fully customize the type mapping process you can provide a custom type mapper implementation by extending the `DefaultArangoTypeMapper` class.

### Deactivating type mapping
To deactivate the type mapping process, you can return `null` from the `typeKey()` method of the `AbstractArangoConfiguration` class. No type hints are stored in the documents with this setting. If you make sure that each defined type corresponds to the actual type, you can disable the type mapping, otherwise it can lead to exceptions when reading the entities from the DB.

## Annotations

### Annotation overview

annotation | level | description
-----------|-------|------------
@Document | class | marks this class as a candidate for mapping
@Edge | class | marks this class as a candidate for mapping
@Id | field | stores the field as the system field _id
@Key | field | stores the field as the system field _key
@Rev | field | stores the field as the system field _rev
@Field("alt-name") | field | stores the field with an alternative name
@Ref | field | stores the _id of the referenced document and not the nested document
@From | field | stores the _id of the referenced document as the system field _from
@To | field | stores the _id of the referenced document as the system field _to
@Relations | field | vertices which are connected over edges
@Transient | field, method, annotation | marks a field to be transient for the mapping framework, thus the property will not be persisted and not further inspected by the mapping framework
@PersistenceConstructor | constructor | marks a given constructor - even a package protected one - to use when instantiating the object from the database
@TypeAlias("alias") | class | set a type alias for the class when persisted to the DB
@HashIndex | class | describes a hash index
@HashIndexed | field | describes how to index the field
@SkiplistIndex | class | describes a skiplist index
@SkiplistIndexed | field | describes how to index the field
@PersistentIndex | class | describes a persistent index
@PersistentIndexed | field | describes how to index the field
@GeoIndex | class | describes a geo index
@GeoIndexed | field | describes how to index the field
@FulltextIndex | class | describes a fulltext index
@FulltextIndexed | field | describes how to index the field

### Document

The annotations `@Document` applied to a class marks this class as a candidate for mapping to the database. The most relevant parameter is `value` to specify the collection name in the database. The annotation `@Document` specifies the collection type to `DOCUMENT`.

```java
@Document(value="persons")
public class Person {
  ...
}
```

### Edge

The annotations `@Edge` applied to a class marks this class as a candidate for mapping to the database. The most relevant parameter is `value` to specify the collection name in the database. The annotation `@Edge` specifies the collection type to `EDGE`.

```java
@Edge("relations")
public class Relation {
  ...
}
```

### Reference

With the annotation `@Ref` applied on a field the nested object isn’t stored as a nested object in the document. The `_id` field of the nested object is stored in the document and the nested object has to be stored as a separate document in another collection described in the `@Document` annotation of the nested object class. To successfully persist an instance of your object the referencing field has to be null or it's instance has to provide a field with the annotation `@Id` including a valid id.

```java
@Document(value="persons")
public class Person {
  @Ref
  private Address address;
}

@Document("addresses")
public class Address {
  @Id
  private String id;
  private String country;
  private String street;
}
```

The database representation of `Person` in collection *persons* looks as follow:

```
{
  "_key" : "123",
  "_id" : "persons/123",
  "address" : "addresses/456"
}
```
and the representation of `Address` in collection *addresses*:
```
{
  "_key" : "456",
  "_id" : "addresses/456",
  "country" : "...",
  "street" : "..."
}
```

Without the annotation `@Ref` at the field `address`, the stored document would look:

```
{
  "_key" : "123",
  "_id" : "persons/123",
  "address" : {
    "country" : "...",
     "street" : "..."
  }
}
```

### Relations

With the annotation `@Relations` applied on a collection or array field in a class annotated with `@Document` the nested objects are fetched from the database over a graph traversal with your current object as the starting point. The most relevant parameter is `edge`. With `edge` you define the edge collection - which should be used in the traversal - using the class type. With the parameter `depth` you can define the maximal depth for the traversal (default 1) and the parameter `direction` defines whether the traversal should follow outgoing or incoming edges (default Direction.ANY).

```java
@Document(value="persons")
public class Person {
  @Relations(edge=Relation.class, depth=1, direction=Direction.ANY)
  private List<Person> friends;
}

@Edge(name="relations")
public class Relation {

}
```

### Document with From and To

With the annotations `@From` and `@To` applied on a collection or array field in a class annotated with `@Document` the nested edge objects are fetched from the database. Each of the nested edge objects has to be stored as separate edge document in the edge collection described in the `@Edge` annotation of the nested object class with the *_id* of the parent document as field *_from* or *_to*.

```java
@Document("persons")
public class Person {
  @From
  private List<Relation> relations;
}

@Edge(name="relations")
public class Relation {
  ...
}
```

The database representation of `Person` in collection *persons* looks as follow:
```
{
  "_key" : "123",
  "_id" : "persons/123"
}
```

and the representation of `Relation` in collection *relations*:
```
{
  "_key" : "456",
  "_id" : "relations/456",
  "_from" : "persons/123"
  "_to" : ".../..."
}
{
  "_key" : "789",
  "_id" : "relations/456",
  "_from" : "persons/123"
  "_to" : ".../..."
}
...

```

### Edge with From and To

With the annotations `@From` and `@To` applied on a field in a class annotated with `@Edge` the nested object is fetched from the database. The nested object has to be stored as a separate document in the collection described in the `@Document` annotation of the nested object class. The *_id* field of this nested object is stored in the fields `_from` or `_to` within the edge document.

```java
@Edge("relations")
public class Relation {
  @From
  private Person c1;
  @To
  private Person c2;
}

@Document(value="persons")
public class Person {
  @Id
  private String id;
}
```

The database representation of `Relation` in collection *relations* looks as follow:
```
{
  "_key" : "123",
  "_id" : "relations/123",
  "_from" : "persons/456",
  "_to" : "persons/789"
}
```

and the representation of `Person` in collection *persons*:
```
{
  "_key" : "456",
  "_id" : "persons/456",
}
{
  "_key" : "789",
  "_id" : "persons/789",
}
```

**Note:** If you want to save an instance of `Relation`, both `Person` objects (from & to) already have to be persisted and the class `Person` needs a field with the annotation `@Id` so it can hold the persisted `_id` from the database.

### Index and Indexed annotations

With the `@<IndexType>Indexed` annotations user defined indexes can be created at a collection level by annotating single fields of a class.

Possible `@<IndexType>Indexed` annotations are:
* `@HashIndexed`
* `@SkiplistIndexed`
* `@PersistentIndexed`
* `@GeoIndexed`
* `@FulltextIndexed`

The following example creates a hash index on the field `name` and a separate hash index on the field `age`:
```java
public class Person {
  @HashIndexed
  private String name;

  @HashIndexed
  private int age;
}
```

With the `@<IndexType>Indexed` annotations different indexes can be created on the same field.

The following example creates a hash index and also a skiplist index on the field `name`:
```java
public class Person {
  @HashIndexed
  @SkiplistIndexed
  private String name;
}
```

If the index should include multiple fields the `@<IndexType>Index` annotations can be used on the type instead.

Possible `@<IndexType>Index` annotations are:
* `@HashIndex`
* `@SkiplistIndex`
* `@PersistentIndex`
* `@GeoIndex`
* `@FulltextIndex`

The following example creates a single hash index on the fields `name` and `age`, note that if a field is renamed in the database with @Field, the new field name must be used in the index declaration:
```java
@HashIndex(fields = {"fullname", "age"})
public class Person {
  @Field("fullname")
  private String name;

  private int age;
}
```

The `@<IndexType>Index` annotations can also be used to create an index on a nested field.

The following example creates a single hash index on the fields `name` and `address.country`:
```java
@HashIndex(fields = {"name", "address.country"})
public class Person {
  private String name;

  private Address address;
}
```

The `@<IndexType>Index` annotations and the `@<IndexType>Indexed` annotations can be used at the same time in one class.

The following example creates a hash index on the fields `name` and `age` and a separate hash index on the field `age`:
```java
@HashIndex(fields = {"name", "age"})
public class Person {
  private String name;

  @HashIndexed
  private int age;
}
```

The `@<IndexType>Index` annotations can be used multiple times to create more than one index in this way.

The following example creates a hash index on the fields `name` and `age` and a separate hash index on the fields `name` and `gender`:
```java
@HashIndex(fields = {"name", "age"})
@HashIndex(fields = {"name", "gender"})
public class Person {
  private String name;

  private int age;

  private Gender gender
}
```
