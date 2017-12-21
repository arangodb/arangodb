Audit Events
============

__This feature is available in the Enterprise Edition.__

Authentication
--------------

### Unknown authentication methods

```
2016-10-03 15:44:23 | server1 | - | database1 | 127.0.0.1:61525 | - | unknown authentication method | /_api/version
```

### Missing credentials

```
2016-10-03 15:39:49 | server1 | - | database1 | 127.0.0.1:61498 | - | credentials missing | /_api/version
```

### Wrong credentials

```
2016-10-03 15:47:26 | server1 | user1 | database1 | 127.0.0.1:61528 | http basic | credentials wrong | /_api/version
```

### Password change required

```
2016-10-03 16:18:53 | server1 | user1 | database1 | 127.0.0.1:62257 | - | password change required | /_api/version
```

### JWT login succeeded

```
2016-10-03 17:21:22 | server1 | - | database1 | 127.0.0.1:64214 | http jwt | user 'root' authenticated | /_open/auth
```

Please note, that the user given as third part is the user that requested
the login. In general, it will be empty.

### JWT login failed

```
2016-10-03 17:21:22 | server1 | - | database1 | 127.0.0.1:64214 | http jwt | user 'root' wrong credentials  | /_open/auth
```

Please note, that the user given as third part is the user that requested
the login. In general, it will be empty.

Authorization
-------------

### User not authorized to access database

```
2016-10-03 16:20:52 | server1 | user1 | database2 | 127.0.0.1:62262 | http basic | not authorized | /_api/version
```

Databases
---------

### Create a database

```
2016-10-04 15:33:25 | server1 | user1 | database1 | 127.0.0.1:56920 | http basic | create database 'database1' | ok | /_api/database
```

### Drop a database

```
2016-10-04 15:33:25 | server1 | user1 | database1 | 127.0.0.1:56920 | http basic | delete database 'database1' | ok | /_api/database
```

Collections
-----------

### Create a collection

```
2016-10-05 17:35:57 | server1 | user1 | database1 | 127.0.0.1:51294 | http basic | create collection 'collection1' | ok | /_api/collection
```

### Truncate a collection

```
2016-10-05 17:36:08 | server1 | user1 | database1 | 127.0.0.1:51294 | http basic | truncate collection 'collection1' | ok | /_api/collection/collection1/truncate
```

### Drop a collection

```
2016-10-05 17:36:30 | server1 | user1 | database1 | 127.0.0.1:51294 | http basic | delete collection 'collection1' | ok | /_api/collection/collection1
```

Indexes
-------

### Create a index

```
2016-10-05 18:19:40 | server1 | user1 | database1 | 127.0.0.1:52467 | http basic | create index in 'collection1' | ok | {"fields":["a"],"sparse":false,"type":"skiplist","unique":false} | /_api/index?collection=collection1
```

### Drop a index

```
2016-10-05 18:18:28 | server1 | user1 | database1 | 127.0.0.1:52464 | http basic | drop index ':44051' | ok | /_api/index/collection1/44051
```

Documents
---------

### Reading a single document

```
2016-10-04 12:27:55 | server1 | user1 | database1 | 127.0.0.1:53699 | http basic | create document ok | /_api/document/collection1
```

### Replacing a single document

```
2016-10-04 12:28:08 | server1 | user1 | database1 | 127.0.0.1:53699 | http basic | replace document ok | /_api/document/collection1/21456?ignoreRevs=false
```

### Modifying a single document

```
2016-10-04 12:28:15 | server1 | user1 | database1 | 127.0.0.1:53699 | http basic | modify document ok | /_api/document/collection1/21456?keepNull=true&ignoreRevs=false
```

### Deleting a single document

```
2016-10-04 12:28:23 | server1 | user1 | database1 | 127.0.0.1:53699 | http basic | delete document ok | /_api/document/collection1/21456?ignoreRevs=false
```

For example, if someones tries to delete a non-existing document, it will be logged as

```
2016-10-04 12:28:26 | server1 | user1 | database1 | 127.0.0.1:53699 | http basic | delete document failed | /_api/document/collection1/21456?ignoreRevs=false
```

Queries
-------

```
2016-10-06 12:12:10 | server1 | user1 | database1 | 127.0.0.1:54232 | http basic | query document | ok | for i in collection1 return i | /_api/cursor
```
