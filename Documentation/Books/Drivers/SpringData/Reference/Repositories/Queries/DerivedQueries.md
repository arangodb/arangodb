<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Derived queries

## Semantic parts

Spring Data ArangoDB supports queries derived from methods names by splitting it into its semantic parts and converting into AQL. The mechanism strips the prefixes `find..By`, `get..By`, `query..By`, `read..By`, `stream..By`, `count..By`, `exists..By`, `delete..By`, `remove..By` from the method and parses the rest. The `By` acts as a separator to indicate the start of the criteria for the query to be built. You can define conditions on entity properties and concatenate them with `And` and `Or`.

The complete list of part types for derived methods is below, where `doc` is a document in the database

| Keyword                                     | Sample                                 | Predicate                              |
| ------------------------------------------- | -------------------------------------- | -------------------------------------- |
| IsGreaterThan, GreaterThan, After           | findByAgeGreaterThan(int age)          | doc.age > age                          |
| IsGreaterThanEqual, GreaterThanEqual        | findByAgeIsGreaterThanEqual(int age)   | doc.age >= age                         |
| IsLessThan, LessThan, Before                | findByAgeIsLessThan(int age)           | doc.age < age                          |
| IsLessThanEqualLessThanEqual                | findByAgeLessThanEqual(int age)        | doc.age <= age                         |
| IsBetween, Between                          | findByAgeBetween(int lower, int upper) | lower < doc.age < upper                |
| IsNotNull, NotNull                          | findByNameNotNull()                    | doc.name != null                       |
| IsNull, Null                                | findByNameNull()                       | doc.name == null                       |
| IsLike, Like                                | findByNameLike(String name)            | doc.name LIKE name                     |
| IsNotLike, NotLike                          | findByNameNotLike(String name)         | NOT(doc.name LIKE name)                |
| IsStartingWith, StartingWith, StartsWith    | findByNameStartsWith(String prefix)    | doc.name LIKE prefix                   |
| IsEndingWith, EndingWith, EndsWith          | findByNameEndingWith(String suffix)    | doc.name LIKE suffix                   |
| Regex, MatchesRegex, Matches                | findByNameRegex(String pattern)        | REGEX_TEST(doc.name, name, ignoreCase) |
| (No Keyword)                                | findByFirstName(String name)           | doc.name == name                       |
| IsTrue, True                                | findByActiveTrue()                     | doc.active == true                     |
| IsFalse, False                              | findByActiveFalse()                    | doc.active == false                    |
| Is, Equals                                  | findByAgeEquals(int age)               | doc.age == age                         |
| IsNot, Not                                  | findByAgeNot(int age)                  | doc.age != age                         |
| IsIn, In                                    | findByNameIn(String[] names)           | doc.name IN names                      |
| IsNotIn, NotIn                              | findByNameIsNotIn(String[] names)      | doc.name NOT IN names                  |
| IsContaining, Containing, Contains          | findByFriendsContaining(String name)   | name IN doc.friends                    |
| IsNotContaining, NotContaining, NotContains | findByFriendsNotContains(String name)  | name NOT IN doc.friends                |
| Exists                                      | findByFriendNameExists()               | HAS(doc.friend, name)                  |

**Examples**

```java
public interface MyRepository extends ArangoRepository<Customer, String> {

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
public interface MyRepository extends ArangoRepository<Customer, String> {

  // FOR c IN customers
  // FILTER c.name == @0
  // SORT c.age DESC RETURN c
  ArangoCursor<Customer> getByNameOrderByAgeDesc(String name);

  // FOR c IN customers
  // FILTER c.name = @0
  // SORT c.name ASC, c.age DESC RETURN c
  ArangoCursor<Customer> findByNameOrderByNameAscAgeDesc(String name);

}
```

## Property expression

Property expressions can refer only to direct and nested properties of the managed domain class. The algorithm checks the domain class for the entire expression as the property. If the check fails, the algorithm splits up the expression at the camel case parts from the right and tries to find the corresponding property.

**Examples**

```java
@Document("customers")
public class Customer {
  private Address address;
}

public class Address {
  private ZipCode zipCode;
}

public interface MyRepository extends ArangoRepository<Customer, String> {

  // 1. step: search domain class for a property "addressZipCode"
  // 2. step: search domain class for "addressZip.code"
  // 3. step: search domain class for "address.zipCode"
  ArangoCursor<Customer> findByAddressZipCode(ZipCode zipCode);
}
```

It is possible for the algorithm to select the wrong property if the domain class also has a property which matches the first split of the expression. To resolve this ambiguity you can use `_` as a separator inside your method-name to define traversal points.

**Examples**

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

public interface MyRepository extends ArangoRepository<Customer, String> {

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

## Geospatial queries

Geospatial queries are a subsection of derived queries. To use a geospatial query on a collection, a geo index must exist on that collection. A geo index can be created on a field which is a two element array, corresponding to latitude and longitude coordinates.

As a subsection of derived queries, geospatial queries support all the same return types, but also support the three return types `GeoPage, GeoResult and Georesults`. These types must be used in order to get the distance of each document as generated by the query.

There are two kinds of geospatial query, Near and Within. Near sorts documents by distance from the given point, while within both sorts and filters documents, returning those within the given distance range or shape.

**Examples**

```java
public interface MyRepository extends ArangoRepository<City, String> {

    GeoResult<City> getByLocationNear(Point point);

    GeoResults<City> findByLocationWithinOrLocationWithin(Box box, Polygon polygon);

    //Equivalent queries
    GeoResults<City> findByLocationWithinOrLocationWithin(Point point, int distance);
    GeoResults<City> findByLocationWithinOrLocationWithin(Point point, Distance distance);
    GeoResults<City> findByLocationWithinOrLocationWithin(Circle circle);

}
```
