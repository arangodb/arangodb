# Howto produce an Agency Dump

One can read out all information of an _Agency_ in the following way:

```
curl -L http://<server>:<port>/_api/agency/read -d '[["/"]]'
```
Please make sure to use the _IP_ and _PORT_ of one _Agent_

The `-L` means that it follows redirections in case one talks to a _follower_ instead of the _leader_ of the _Agency_.

In case of an authenticated _Cluster_, we need a JWT token to access _Agents_. 

The user provides a "JWT-secret" to every server via the options (command line or config file). Let's say the JWT-secret is _geheim_. Then this is turned into a token. This can be done in the following way:

```
jwtgen -a HS256 -s geheim -c server_id=setup -c iss=arangodb
```

which outputs this:

```
eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpYXQiOjE1Mjc2ODYzMDAsInNlcnZlcl9pZCI6InNldHVwIiwiaXNzIjoiYXJhbmdvZGIifQ.dBUhmxY3Q7rLHHDQc9FL4ghOfGiNJRFws_U2ZX4H-58
```
   
Note that `iss` values is essentially arbitrary, the crucial thing are the `HS256`, the JWT secret `geheim` and `server_id=setup`. This needs a node.js program called `jwtgen` which one can install on a system with `node.js` as follows:

```
npm install -g jwtgen
```

This token is then used in the following way with `curl`, to produce the _Agency_ dump:

```
curl -L http://<server>:<port>/_api/agency/read -d '[["/"]]' -H "Authorization: bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpYXQiOjE1Mjc2ODYzMDAsInNlcnZlcl9pZCI6InNldHVwIiwiaXNzIjoiYXJhbmdvZGIifQ.dBUhmxY3Q7rLHHDQc9FL4ghOfGiNJRFws_U2ZX4H-58"
```

Please make sure to use the _IP_ and _PORT_ of one _Agent_.

The above can be easily executed in a single command:

```
curl -L http://<server>:<port>/_api/agency/read [["/"]]' -H "Authorization: bearer $(jwtgen -a H256 -s geheim -c 'iss=arangodb' -c 'server_id=setup')"
```

Please make sure to use the _IP_ and _PORT_ of one _Agent_.
