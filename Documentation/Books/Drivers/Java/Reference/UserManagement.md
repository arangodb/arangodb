<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# User management

If you are using [authentication](https://docs.arangodb.com/Manual/GettingStarted/Authentication.html) you can manage users with the driver.

## add user

```Java
  //username, password
  arangoDB.createUser("myUser", "myPassword");
```

## delete user

```Java
  arangoDB.deleteUser("myUser");
```

## list users

```Java
  Collection<UserResult> users = arangoDB.getUsers();
  for(UserResult user : users) {
    System.out.println(user.getUser())
  }
```

## grant user access

```Java
  arangoDB.db("myDatabase").grantAccess("myUser");
```

## revoke user access

```Java
  arangoDB.db("myDatabase").revokeAccess("myUser");
```
