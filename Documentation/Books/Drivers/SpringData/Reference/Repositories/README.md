<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Repositories

Spring Data Commons provides a composable repository infrastructure which Spring Data ArangoDB is built on. These allow for interface-based composition of repositories consisting of provided default implementations for certain interfaces (like `CrudRepository`) and custom implementations for other methods.

The base interface of Spring Data ArangoDB is `ArangoRepository`. It extends the Spring Data interfaces `PagingAndSortingRepository` and `QueryByExampleExecutor`. To get access to all Sping Data ArangoDB repository functionallity simply create your own interface extending `ArangoRepository<T, ID>`.

The type `T` represents your domain class and type `ID` the type of your field annotated with `@Id` in your domain class. This field is persistend in ArangoDB as document field `_key`.

**Examples**

```java
@Document
public class MyDomainClass {
  @Id
  private String id;

}

public interface MyRepository extends ArangoRepository<MyDomainClass, String> {

}
```

Instances of a Repository are created in Spring beans through the auto-wired mechanism of Spring.

```java
public class MySpringBean {

  @Autowired
  private MyRepository rep;

}
```
