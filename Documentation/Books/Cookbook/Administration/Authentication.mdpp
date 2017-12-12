# Using authentication

## Problem

I want to use authentication in ArangoDB.

## Solution

In order to make authentication work properly, you will need to create user accounts first.

Then adjust ArangoDB's configuration and turn on authentication (if it's off).

### Set up or adjust user accounts

ArangoDB user accounts are valid throughout a server instance and users can be granted
access to one or more databases. They are managed through the database named `_system`.

To manage user accounts, connect with the ArangoShell to the ArangoDB host and the 
`_system` database:

```
$ arangosh --server.endpoint tcp://127.0.0.1:8529 --server.database "_system"
```

By default, arangosh will connect with a username `root` and an empty password. This
will work if authentication is turned off.

When connected, you can create a new user account with the following command:

``` 
arangosh> require("org/arangodb/users").save("myuser", "mypasswd");
```

`myuser` will be the username and `mypasswd` will be the user's password. Note that running
the command like this may store the password literally in ArangoShell's history.

To avoid that, use a dynamically created password, e.g.:

``` 
arangosh> passwd = require("internal").genRandomAlphaNumbers(20);
arangosh> require("org/arangodb/users").save("myuser", passwd);
```

The above will print the password on screen (so you can memorize it) but won't store
it in the command history.

While there, you probably want to change the password of the default `root` user too.
Otherwise one will be able to connect with the default `root` user and its
empty password. The following commands change the `root` user's password:

``` 
arangosh> passwd = require("internal").genRandomAlphaNumbers(20);
arangosh> require("org/arangodb/users").update("root", passwd);
```

### Turn on authentication

Authentication is turned on by default in ArangoDB. You should make sure that it was
not turned off manually however. Check the configuration file (normally named
`/etc/arangodb.conf`) and make sure it contains the following line in the `server` section:

```
authentication = true
```

This will make ArangoDB require authentication for every request (including requests to
Foxx apps). 

If you want to run Foxx apps without HTTP authentcation, but activate HTTP authentication 
for the built-in server APIs, you can add the following line in the `server` section of 
the configuration:

```
authentication-system-only = true
```

The above will bypass authentication for requests to Foxx apps.

When finished making changes, you need to restart ArangoDB:

```
service arangodb restart
```

### Check accessibility

To confirm authentication is in effect, try connecting to ArangoDB with the ArangoShell:

```
$ arangosh --server.endpoint tcp://127.0.0.1:8529 --server.database "_system"
```

The above will implicity use a username `root` and an empty password when connecting. If
you changed the password of the `root` account as described above, this should not work anymore.

You should also validate that you can connect with a valid user:

```
$ arangosh --server.endpoint tcp://127.0.0.1:8529 --server.database "_system" --server.username myuser
```

You can also use curl to check that you are actually getting HTTP 401 (Unauthorized) server
responses for requests that require authentication:

```
$ curl --dump - http://127.0.0.1:8529/_api/version
```

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #authentication #security
