<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Auditing

Since version 3.0.0 Spring Data ArangoDB provides basic auditing functionallity where you can track who made changes on your data and when.

To enable auditing you have to add the annotation `@EnableArangoAuditing` to your configuration class.

```Java
@Configuration
@EnableArangoAuditing
public class MyConfiguration extends AbstractArangoConfiguration {
```

We can now add fields to our model classes and annotade them with `@CreateDate`, `@CreatedBy`, `@LastModifiedDate` and `@LastModifiedBy` to store the auditing information. All annotation names should be self-explanatory.

```Java
@Document
public class MyEntity {

  @CreatedDate
  private Instant created;

  @CreatedBy
  private User createdBy;

  @LastModifiedDate
  private Instant modified;

  @LastModifiedBy
  private User modifiedBy;

}
```

The annotations `@CreateDate` and `@LastModifiedDate` are working with fields of any kind of Date/Timestamp type which is supported by Spring Data. (i.e. `java.util.Date`, `java.time.Instant`, `java.time.LocalDateTime`).

For `@CreatedBy` and `@LastModifiedBy` we need to provide Spring Data the information of the current auditor (i.e. `User` in our case). We can do so by implementing the `AuditorAware` interface

```Java
public class AuditorProvider implements AuditorAware<User> {
  @Override
  public Optional<User> getCurrentAuditor() {
    // return current user
  }
}
```

and add the implementation as a bean to our Spring context.

```Java
@Configuration
@EnableArangoAuditing(auditorAwareRef = "auditorProvider")
public class MyConfiguration extends AbstractArangoConfiguration {

  @Bean
  public AuditorAware<User> auditorProvider() {
    return new AuditorProvider();
  }

}
```

If you use a type in your `AuditorAware` implementation, which will be also persisted in your database and you only want to save a reference in your entity, just add the [@Ref annotation](Reference.md) to the fields annotated with `@CreatedBy` and `@LastModifiedBy`. Keep in mind that you have to save the `User` in your database first to get a valid reference.

```Java
@Document
public class MyEntity {

  @Ref
  @CreatedBy
  private User createdBy;

  @Ref
  @LastModifiedBy
  private User modifiedBy;

}
```
