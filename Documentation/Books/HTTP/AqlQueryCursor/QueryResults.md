Retrieving query results
========================

Select queries are executed on-the-fly on the server and the result
set will be returned back to the client.

There are two ways the client can get the result set from the server:

* In a single roundtrip
* Using a cursor

### Single roundtrip

The server will only transfer a certain number of result documents back to the
client in one roundtrip. This number is controllable by the client by setting
the *batchSize* attribute when issuing the query.

If the complete result can be transferred to the client in one go, the client
does not need to issue any further request. The client can check whether it has
retrieved the complete result set by checking the *hasMore* attribute of the
result set. If it is set to *false*, then the client has fetched the complete
result set from the server. In this case no server side cursor will be created.

```js
> curl --data @- -X POST --dump - http://localhost:8529/_api/cursor
{ "query" : "FOR u IN users LIMIT 2 RETURN u", "count" : true, "batchSize" : 2 }

HTTP/1.1 201 Created
Content-type: application/json

{
  "hasMore" : false,
  "error" : false,
  "result" : [
    {
      "name" : "user1",
      "_rev" : "210304551",
      "_key" : "210304551",
      "_id" : "users/210304551"
    },
    {
      "name" : "user2",
      "_rev" : "210304552",
      "_key" : "210304552",
      "_id" : "users/210304552"
    }
  ],
  "code" : 201,
  "count" : 2
}
```

### Using a cursor

If the result set contains more documents than should be transferred in a single
roundtrip (i.e. as set via the *batchSize* attribute), the server will return
the first few documents and create a temporary cursor. The cursor identifier
will also be returned to the client. The server will put the cursor identifier
in the *id* attribute of the response object. Furthermore, the *hasMore*
attribute of the response object will be set to *true*. This is an indication
for the client that there are additional results to fetch from the server.

*Examples*:

Create and extract first batch:

```js
> curl --data @- -X POST --dump - http://localhost:8529/_api/cursor
{ "query" : "FOR u IN users LIMIT 5 RETURN u", "count" : true, "batchSize" : 2 }

HTTP/1.1 201 Created
Content-type: application/json

{
  "hasMore" : true,
  "error" : false,
  "id" : "26011191",
  "result" : [
    {
      "name" : "user1",
      "_rev" : "258801191",
      "_key" : "258801191",
      "_id" : "users/258801191"
    },
    {
      "name" : "user2",
      "_rev" : "258801192",
      "_key" : "258801192",
      "_id" : "users/258801192"
    }
  ],
  "code" : 201,
  "count" : 5
}
```

Extract next batch, still have more:

```js
> curl -X PUT --dump - http://localhost:8529/_api/cursor/26011191

HTTP/1.1 200 OK
Content-type: application/json

{
  "hasMore" : true,
  "error" : false,
  "id" : "26011191",
  "result": [
    {
      "name" : "user3",
      "_rev" : "258801193",
      "_key" : "258801193",
      "_id" : "users/258801193"
    },
    {
      "name" : "user4",
      "_rev" : "258801194",
      "_key" : "258801194",
      "_id" : "users/258801194"
    }
  ],
  "code" : 200,
  "count" : 5
}
```

Extract next batch, done:

```js
> curl -X PUT --dump - http://localhost:8529/_api/cursor/26011191

HTTP/1.1 200 OK
Content-type: application/json

{
  "hasMore" : false,
  "error" : false,
  "result" : [
    {
      "name" : "user5",
      "_rev" : "258801195",
      "_key" : "258801195",
      "_id" : "users/258801195"
    }
  ],
  "code" : 200,
  "count" : 5
}
```

Do not do this because *hasMore* now has a value of false:

```js
> curl -X PUT --dump - http://localhost:8529/_api/cursor/26011191

HTTP/1.1 404 Not Found
Content-type: application/json

{
  "errorNum": 1600,
  "errorMessage": "cursor not found: disposed or unknown cursor",
  "error": true,
  "code": 404
}
```

### Modifying documents

The `_api/cursor` endpoint can also be used to execute modifying queries.

The following example appends a value into the array `arrayValue` of the document
with key `test` in the collection `documents`. Normal update behavior is to
replace the attribute completely, and using an update AQL query with the `PUSH()` 
function allows to append to the array.

```js
curl --data @- -X POST --dump http://127.0.0.1:8529/_api/cursor
{ "query": "FOR doc IN documents FILTER doc._key == @myKey UPDATE doc._key WITH { arrayValue: PUSH(doc.arrayValue, @value) } IN documents","bindVars": { "myKey": "test", "value": 42 } }

HTTP/1.1 201 Created
Content-type: application/json; charset=utf-8

{
  "result" : [],
  "hasMore" : false,
  "extra" : {
    "stats" : {
      "writesExecuted" : 1,
      "writesIgnored" : 0,
      "scannedFull" : 0,
      "scannedIndex" : 1,
      "filtered" : 0
    },
    "warnings" : []
  },
  "error" : false,
  "code" : 201
}
```
