<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Foxx

## call a service

```Java
  Request request = new Request("mydb", RequestType.GET, "/my/foxx/service")
  Response response = arangoDB.execute(request);
```
