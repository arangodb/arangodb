# Howto Reset Root Password

One can reset Root Password in the following way:

1) Stop the server
2) Set `authentication=false` in the `arangod.conf` file
3) Restart the server
4) Change the password using the following command via `Arangosh`

```

require("org/arangodb/users").update("root", "newpassword");

```
This command should return:

```

{
  "user" : "root",
  "active" : true,
  "extra" : {
  },
  "code" : 200
}

```
5) Set `authentication=true` from `arangod.conf` file
6) Restart the server
7) Test the connection 

Please note this procedure is meant for _Single Instance_ if you are using an _ArangoDB Cluster_ or _Active Failover_ you should set disable and enable authentication from the `arangod.conf` file of each node. Changes to the `arangod.conf file` under the path  `ect/arangodb3/arangod.conf` in _Cluster_ and _Active Failover_ deployment will not work in this case.