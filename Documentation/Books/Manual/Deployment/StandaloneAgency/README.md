Launching ArangoDB's standalone _Agency_
========================================

Multiple ArangoDB instances can be deployed as a fault-tolerant distributed state machine.

What is a fault-tolerant state machine in the first place?

In many service deployments consisting of arbitrary components distributed over multiple machines one is faced with the challenge of creating a dependable centralized knowledge base or configuration. Implementation of such a service turns out to be one of the most fundamental problems in information engineering. While it may seem as if the realization of such a service is easily conceivable, dependability formulates a paradox on computer networks per se. On the one hand, one needs a distributed system to avoid a single point of failure. On the other hand, one has to establish consensus among the computers involved.

Consensus is the keyword here and its realization on a network proves to be far from trivial. Many papers and conference proceedings have discussed and evaluated this key challenge. Two algorithms, historically far apart, have become widely popular, namely Paxos and its derivatives and Raft. Discussing them and their differences, although highly enjoyable, must remain far beyond the scope of this document. Find the references to the main publications at the bottom of this page.

At ArangoDB, we decided to implement Raft as it is arguably the easier to understand and thus implement. In simple terms, Raft guarantees that a linear stream of transactions, is replicated in realtime among a group of machines through an elected leader, who in turn must have access to and project leadership upon an overall majority of participating instances. In ArangoDB we like to call the entirety of the components of the replicated transaction log, that is the machines and the ArangoDB instances, which constitute the replicated log, the agency.

Startup
-------

The agency must consists of an odd number of agents in order to be able to establish an overall majority and some means for the agents to be able to find one another at startup. 

The most obvious way would be to inform all agents of the addresses and ports of the rest. This however, is more information than needed. For example, it would suffice, if all agents would know the address and port of the next agent in a cyclic fashion. Another straightforward solution would be to inform all agents of the address and port of say the first agent.

Clearly all cases, which would form disjunct subsets of agents would break or in the least impair the functionality of the agency. From there on the agents will gossip the missing information about their peers.

Typically, one achieves fairly high fault-tolerance with low, odd number of agents while keeping the necessary network traffic at a minimum. It seems that the typical agency size will be in range of 3 to 7 agents.

The below commands start up a 3-host agency on one physical/logical box with ports 8529, 8530 and 8531 for demonstration purposes. The address of the first instance, port 8529, is known to the other two. After at most 2 rounds of gossipping, the last 2 agents will have a complete picture of their surrounding and persist it for the next restart.

```
./arangod --agency.activate true --agency.size 3 --agency.my-address tcp://localhost:8531 --server.authentication false --server.endpoint tcp://0.0.0.0:8531 agency-8531
./arangod --agency.activate true --agency.size 3 --agency.my-address tcp://localhost:8541 --server.authentication false --server.endpoint tcp://0.0.0.0:8541 --agency.endpoint tcp://localhost:8531 agency-8541
./arangod --agency.activate true --agency.size 3 --agency.my-address tcp://localhost:8551 --server.authentication false --server.endpoint tcp://0.0.0.0:8551 --agency.endpoint tcp://localhost:8531 agency-8551 
```

The parameter `agency.endpoint` is the key ingredient for the second and third instances to find the first instance and thus form a complete agency. Please refer to the the shell-script `scripts/startStandaloneAgency.sh` on github or in the source directory.

Key-value-store API
-------------------

The agency should be up and running within a couple of seconds, during which the instances have gossiped their way into knowing the other agents and elected a leader. The public API can be checked for the state of the configuration:

```
curl -s localhost:8531/_api/agency/config
```

```js
{
  "term": 1,
  "leaderId": "AGNT-cec78b63-f098-4b4e-a157-a7bebf7947ba",
  "commitIndex": 1,
  "lastCompactionAt": 0,
  "nextCompactionAfter": 1000,
  "lastAcked": {
    "AGNT-cec78b63-f098-4b4e-a157-a7bebf7947ba": {
      "lastAckedTime": 0,
      "lastAckedIndex": 1
    },
    "AGNT-5c8d92ed-3fb5-4886-8990-742ddb4482fa": {
      "lastAckedTime": 0.167,
      "lastAckedIndex": 1,
      "lastAppend": 15.173
    },
    "AGNT-f6e79b6f-d55f-4ae5-a5e2-4c2d6272b0b8": {
      "lastAckedTime": 0.167,
      "lastAckedIndex": 1,
      "lastAppend": 15.173
    }
  },
  "configuration": {
    "pool": {
      "AGNT-f6e79b6f-d55f-4ae5-a5e2-4c2d6272b0b8": "tcp://localhost:8551",
      "AGNT-cec78b63-f098-4b4e-a157-a7bebf7947ba": "tcp://localhost:8531",
      "AGNT-5c8d92ed-3fb5-4886-8990-742ddb4482fa": "tcp://localhost:8541"
    },
    "active": [
      "AGNT-f6e79b6f-d55f-4ae5-a5e2-4c2d6272b0b8",
      "AGNT-5c8d92ed-3fb5-4886-8990-742ddb4482fa",
      "AGNT-cec78b63-f098-4b4e-a157-a7bebf7947ba"
    ],
    "id": "AGNT-cec78b63-f098-4b4e-a157-a7bebf7947ba",
    "agency size": 3,
    "pool size": 3,
    "endpoint": "tcp://localhost:8531",
    "min ping": 1,
    "max ping": 5,
    "timeoutMult": 1,
    "supervision": false,
    "supervision frequency": 1,
    "compaction step size": 1000,
    "compaction keep size": 50000,
    "supervision grace period": 10,
    "version": 4,
    "startup": "origin"
  },
  "engine": "rocksdb",
  "version": "3.4.3"
}
```

To highlight some details in the above output look for `"term"` and `"leaderId"`. Both are key information about the current state of the Raft algorithm. You may have noted that the first election term has established a random leader for the agency, who is in charge of replication of the state machine and for all external read and write requests until such time that the process gets isolated from the other two subsequently losing its leadership.

Read and Write APIs
-------------------

Generally, all read and write accesses are transactions moreover any read and write access may consist of multiple such transactions formulated as arrays of arrays in JSON documents.

Read transaction
----------------

An agency started from scratch will deal with the simplest query as follows:
```
curl -L localhost:8529/_api/agency/read -d '[["/"]]'
```

```js
[{}]
```

The above request for an empty key value store will return with an empty document. The inner array brackets will aggregate a result from multiple sources in the key-value-store while the outer array will deliver multiple such aggregated results. Also note the `-L` curl flag, which allows the request to follow redirects to the current leader.

Consider the following key-value-store:
```js
{
  "baz": 12,
  "corge": {
    "e": 2.718281828459045,
    "pi": 3.14159265359
  },
  "foo": {
    "bar": "Hello World"
  },
  "qux": {
    "quux": "Hello World"
  }
}
```

The following array of read transactions will yield:

```
curl -L localhost:8529/_api/agency/read -d '[["/foo", "/foo/bar", "/baz"],["/qux"]]'
```

```js
[
  {
    "baz": 12,
    "foo": {
      "bar": "Hello World"
    }
  },
  {
    "qux": {
      "quux": "Hello World"
    }
  }
]
```

Note that the result is an array of two results for the first and second read transactions from above accordingly. Also note that the results from the first read transaction are aggregated into
```js
{
  "baz": 12,
  "foo": {
    "bar": "Hello World"
  }
}
```

The aggregation is performed on 2 levels:

1. `/foo/bar` is eliminated as a subset of `/foo`
2. The results from `/foo` and `/bar` are joined

The word transaction means here that it is guaranteed that all aggregations happen in quasi-realtime and that no write access could have happened in between.

Btw, the same transaction on the virgin key-value store would produce `[{},{}]`

Write API
---------

The write API must unfortunately be a little more complex. Multiple roads lead to Rome:

```
curl -L localhost:8529/_api/agency/write -d '[[{"/foo":{"op":"push","new":"bar"}}]]'
curl -L localhost:8529/_api/agency/write -d '[[{"/foo":{"op":"push","new":"baz"}}]]'
curl -L localhost:8529/_api/agency/write -d '[[{"/foo":{"op":"push","new":"qux"}}]]'
```

and

```
curl -L localhost:8529/_api/agency/write -d '[[{"foo":["bar","baz","qux"]}]]'
```

are equivalent for example and will create and fill an array at `/foo`. Here, again, the outermost array is the container for the transaction arrays.

A complete guide of the API can be found in the [API section](../../../HTTP/Agency/index.html).

