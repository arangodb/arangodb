How to produce an Agency Dump
=============================

One can read out all information of an _Agency_ in the following way:

```
curl -L http://<server>:<port>/_api/agency/read -d '[["/"]]'
```

Please make sure to use the _IP_ (or hostname) and _PORT_ of one _Agent_.

The `-L` means that the _curl_ request follows redirections in case one talks to a _follower_ instead of the _leader_ of the _Agency_.

In case of an authenticated _Cluster_, to access _Agents_ a JWT token is needed. 

When authentication is enabled, the user provides a "JWT-secret" to every server via the options (command line or config file). 
Let's suppose the JWT-secret is _geheim_. To create the _Agency_ dump, the _secret_ first has to be turned into a token. This can be done in the following way:

```
jwtgen -a HS256 -s geheim -c server_id=setup -c iss=arangodb
```

which outputs the following text:

```
eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpYXQiOjE1Mjc2ODYzMDAsInNlcnZlcl9pZCI6InNldHVwIiwiaXNzIjoiYXJhbmdvZGIifQ.dBUhmxY3Q7rLHHDQc9FL4ghOfGiNJRFws_U2ZX4H-58
```
   
Note that the `iss` values is essentially arbitrary: the crucial things in the _jwtgen_ command above are the `HS256`, the JWT secret `geheim` and `server_id=setup`. 

`jwtgen` is a node.js program that can be installed on a system with `node.js` as follows:

```
npm install -g jwtgen
```

The generated token is then used in the following way with `curl`, to produce the _Agency_ dump:

```
curl -L http://<server>:<port>/_api/agency/read -d '[["/"]]' -H "Authorization: bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpYXQiOjE1Mjc2ODYzMDAsInNlcnZlcl9pZCI6InNldHVwIiwiaXNzIjoiYXJhbmdvZGIifQ.dBUhmxY3Q7rLHHDQc9FL4ghOfGiNJRFws_U2ZX4H-58"
```

Please make sure to use the _IP_ (or hostname) and _PORT_ of one _Agent_.

The two commands above can be easily executed in a single command in the following way:

```
curl -L http://<server>:<port>/_api/agency/read [["/"]]' -H "Authorization: bearer $(jwtgen -a H256 -s geheim -c 'iss=arangodb' -c 'server_id=setup')"
```

As always, use the _IP_ (or hostname) and _PORT_ of one _Agent_.

Should the _Agency_ be down, an _Agency_ dump can still be created starting from the database directory of (one of) the _Agents_. Contact ArangoDB Support, in this case, to obtain more detailed guidelines on how to produce the dump.
