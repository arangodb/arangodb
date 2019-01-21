<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Queries

Spring Data ArangoDB supports three kinds of queries:

- [Derived queries](DerivedQueries.md)
- [Query methods](QueryMethods.md)
- [Named queries](NamedQueries.md)

## Return types

The method return type for single results can be a primitive type, a domain class, `Map<String, Object>`, `BaseDocument`, `BaseEdgeDocument`, `Optional<Type>`, `GeoResult<Type>`.

The method return type for multiple results can additionally be `ArangoCursor<Type>`, `Iterable<Type>`, `Collection<Type>`, `List<Type>`, `Set<Type>`, `Page<Type>`, `Slice<Type>`, `GeoPage<Type>`, `GeoResults<Type>` where Type can be everything a single result can be.

## AQL query options

You can set additional options for the query and the created cursor over the class `AqlQueryOptions` which you can simply define as a method parameter without a specific name. AqlQuery options can also be defined with the `@QueryOptions` annotation, as shown below. Aql query options from an annotation and those from an argument are merged if both exist, with those in the argument taking precedence.

The `AqlQueryOptions` allows you to set the cursor time-to-live, batch-size,
caching flag and several other settings. This special parameter works with both
[query methods](QueryMethods.md)
and [derived queries](DerivedQueries.md). Keep in mind that some options, like
time-to-live, are only effective if the method return type is`ArangoCursor<T>`
or `Iterable<T>`.

**Examples**

```java
public interface MyRepository extends Repository<Customer, String> {


  @Query("FOR c IN #collection FILTER c.name == @0 RETURN c")
  Iterable<Customer> query(String name, AqlQueryOptions options);


  Iterable<Customer> findByName(String name, AqlQueryOptions options);


  @QueryOptions(maxPlans = 1000, ttl = 128)
  ArangoCursor<Customer> findByAddressZipCode(ZipCode zipCode);


  @Query("FOR c IN #collection FILTER c[@field] == @value RETURN c")
  @QueryOptions(cache = true, ttl = 128)
  ArangoCursor<Customer> query(Map<String, Object> bindVars, AqlQueryOptions options);

}
```

## Paging and sorting

Spring Data ArangoDB supports Spring Data's `Pageable` and `Sort` parameters for repository query methods. If these parameters are used together with a native query, either through `@Query` annotation or [named queries](NamedQueries.md), a placeholder must be specified:

- `#pageable` for `Pageable` parameter
- `#sort` for `Sort` parameter

Sort properties or paths are attributes separated by dots (e.g. `customer.age`). Some rules apply for them:

- they must not begin or end with a dot (e.g. `.customer.age`)
- dots in attributes are supported, but the whole attribute must be enclosed by backticks (e.g. `` customer.`attr.with.dots` ``
- backticks in attributes are supported, but they must be escaped with a backslash (e.g. `` customer.attr_with\` ``)
- any backslashes (that do not escape a backtick) are escaped (e.g. `customer\` => `customer\\`)

**Examples**

```
just.`some`.`attributes.that`.`form\``.a path\`.\  is converted to
`just`.`some`.`attributes.that`.`form\``.`a path\``.`\\`
```

**Native queries example**

```java
public interface CustomerRepository extends ArangoRepository<Customer> {

  @Query("FOR c IN #collection FILTER c.name == @1 #pageable RETURN c")
  Page<Customer> findByNameNative(Pageable pageable, String name);

  @Query("FOR c IN #collection FILTER c.name == @1 #sort RETURN c")
  List<Customer> findByNameNative(Sort sort, String name);
}

// don't forget to specify the var name of the document
final Pageable page = PageRequest.of(1, 10, Sort.by("c.age"));
repository.findByNameNative(page, "Matt");

final Sort sort = Sort.by(Direction.DESC, "c.age");
repository.findByNameNative(sort, "Tony");
```

**Derived queries example**

```java
public interface CustomerRepository extends ArangoRepository<Customer> {

  Page<Customer> findByName(Pageable pageable, String name);

  List<Customer> findByName(Sort sort, String name);
}

// no var name is necessary for derived queries
final Pageable page = PageRequest.of(1, 10, Sort.by("age"));
repository.findByName(page, "Matt");

final Sort sort = Sort.by(Direction.DESC, "age");
repository.findByName(sort, "Tony");
```
