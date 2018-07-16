# How-to Reset Root Password

One can reset the _root_ password in the following way:

- Stop the Server
- Set `authentication=false` in the `arangod.conf` file
- Restart the Server
  - **Note:** you might need to take any needed precaution to avoid this server can be accessed from outside as currently authentication is temporarily disabled. You might do this by disabling network access or using _localhost_ for the binding (`--server.endpoint tcp://127.0.0.1:8529`)
-  Change the password using the ArangoDB Web UI, or using the following command via `Arangosh`:

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

- Set `authentication=true` in the `arangod.conf` file
- Restart the server
- Test the connection 

Please note that the above procedure is meant for _Single Instance_. If you are using an _ArangoDB Cluster_ or _Active Failover_ you should disable and enable authentication in the `arangod.conf` file of each node. Changes to the `arangod.conf` file under the path `etc/arangodb3/arangod.conf` in _Cluster_ and _Active Failover_ deployments will not work in this case.
