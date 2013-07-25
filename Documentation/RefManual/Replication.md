Replication Events{#RefManualReplication}
=========================================

@NAVIGATE_RefManualReplication
@EMBEDTOC{RefManualReplicationTOC}

The replication logger in ArangoDB will log all events into the `_replication`
system collection. It will only log events when the logger is enabled.

Continuous Replication Log{#RefManualReplicationContinuous}
===========================================================

Replication log events are made available to replication clients via the API at
`/_api/replication/logger-follow`. This API can be called by clients to fetch
replication log events repeatedly.

The following sections describe in detail the structure of the log events
returned by this API.

Replication Event Types{#RefManualReplicationEventTypes}
--------------------------------------------------------

The following replication event types will be logged by ArangoDB 1.4:

- 1000: the replication logger was stopped
- 1001: the replication logger was started
- 2000: a collection was created
- 2001: a collection was dropped
- 2002: a collection was renamed
- 2003: the properties of a collection were changed
- 2100: an index was created
- 2101: an index was dropped
- 2200: multi-document transaction started
- 2201: multi-document transaction committed
- 2300: document insert/update operation
- 2301: edge insert/update operation
- 2302: document/edge deletion

Each replication event itself is logged as a document with the following
attributes. Only the `type` attribute is mandatory, and the other attributes
are optional. Which attributes are used depends on the replication event type:

- `type`: mandatory. Indicates the type of the replication event (see list above)
- `cid`: optional. The id of the collection that is affected
- `tid`: optional. The id of the transaction the operation belongs to
- `key`: optional. Contains the document key for document operations
- `rev`: optional. Contains the document revision id for document operations
- `data`: optional. Contains the actual document/edge for document operations

When replication events are queried via the `/_api/replication/logger-follow` API,
the `tick` attribute will also be returned for each replication event. The `tick`
value is a sequence number and is used by the replication applier to determine 
whether a replication event was already processed.

Examples{#RefManualReplicationExamples}
---------------------------------------

- 1000: the replication logger was stopped:

  This is an informational event. The `lastTick` attribute indicates the last tick value 
  the replication logger had logged before it was stopped.

      {
        "tick":"247908129523087",
        "type":1000,
        "lastTick":"247908129326479"
      }

- 1001: the replication logger was started:
  
  This is an informational event. The `lastTick` attribute indicates the last tick value 
  the replication logger had logged before it was started.

  The `tick` value is `0` if the logger was never started before:

      {
        "tick":"247908129326479",
        "type":1001,
        "lastTick":"0"
      }

  If the logger was started before, the `tick` value will be non-zero:

      {
        "tick":"247907874870862",
        "type":1001,
        "tick":"247907874674254"
      }

- 2000: a collection was created:

  This event is logged when a collection is created. The `cid` attribute indicates the
  collection's id, and the `collection` attribute contains all details for the collection.

      {
        "tick":"247908131947919",
        "type":2000,
        "cid":"247908131227023",
        "collection": {
          "version":4,
          "type":2,
          "cid":"247908131227023",
          "deleted":false,
          "doCompact":true,
          "maximalSize":33554432,
          "name":"UnitTestsReplication",
          "isVolatile":false,
          "waitForSync":false
        }
      }

- 2001: a collection was dropped:

  This event is logged when a collection is dropped. The `cid` attribute indicates the
  id of the collection that is dropped:

      {
        "tick":"247908135093647",
        "type":2001,
        "cid":"247908134045071"
      }

- 2002: a collection was renamed:

  This event is logged when a collection is renamed. The `cid` attribute contains the
  id of the collection that is renamed. The `collection` attribute contains the `name`
  attribute, which contains the new name for the collection:

      {
        "tick":"247908136469903",
        "type":2002,
        "cid":"247908135421327",
        "collection": {
          "name":"UnitTestsReplication2"
        }
      }

- 2003: the properties of a collection were changed:
  
  This event is logged when the properties of a collection are changed. The `cid` 
  attribute contains the id of the affected collection. The `collection` attribute 
  contains all the current (including the changed) properties. Note that renaming a
  collection will not trigger a property change, but a rename even:

      {
        "tick":"247908139025807",
        "type":2003,
        "cid":"247908137977231",
        "collection": {
          "version":4,
          "type":2,
          "cid":"247908137977231",
          "deleted":false,
          "doCompact":true,
          "maximalSize":2097152,
          "name":"UnitTestsReplication",
          "isVolatile":false,
          "waitForSync":true
        }
      }

- 2100: an index was created:

  This event is logged when a new index is created for a collection. The `cid` attribute
  contains the id of the collection the index is created for. The `index` attribute
  contains all index details. The `index` attribute will have at least the following
  sub-attributes:
  
  - `id`: the index id
  - `type`: the type of the index
  - `unique`: whether or not the index is unique

  All other sub-attributes of the `index` attribute vary depending on the index type.

  Note that as each collection will automatically have a primary index, the creation of
  the primary index will not be logged. For edge collections, the automatic creation of
  the edges index will also not be logged. 

  Some examples for index creation events:

      {
        "tick":"247908237985167",
        "type":2100,
        "cid":"247908236739983",
        "index": {
          "id":"247908237854095",
          "type":"geo2",
          "unique":false,
          "constraint":false,
          "fields": [
            "a",
            "b"
          ]
        }
      }

      {
        "tick":"247908234773903",
        "type":2100,
        "cid":"247908233594255",
        "index": {
          "id":"247908234642831",
          "type":"skiplist",
          "unique":true,
          "fields": [
            "a"
          ]
        }
      }

      {
        "tick":"247908229924239",
        "type":2100,
        "cid":"247908228679055",
        "index": {
          "id":"247908229793167",
          "type":"hash",
          "unique":true,
          "fields": [
            "a",
            "b"
          ]
        }
      }

      {
        "tick":"247908245915023",
        "type":2100,
        "cid":"247908244800911",
        "index": {
          "id":"247908245783951",
          "type":"cap",
          "unique":false,
          "size":100,
          "byteSize":0
        }
      }

- 2101: an index was dropped.
  
  This event is logged when an index of a collection is dropped. The `cid` attribute
  contains the id of the collection the index is dropped for. The `id` attribute
  contains the id of the dropped index:

      {
        "tick":"247908249126287",
        "type":2101,
        "cid":"247908247815567",
        "id":"247908248667535"
      }

- 2200: multi-document transaction started:

  This event is logged at the start of a multi-document and/or multi-collection 
  transaction. The `tid` attribute contains the transaction id. The `collections`
  attribute contains information about which collections are part of the transaction.
  
  The `collections` attribute is a list of entries with the following attributes each:
 
  - `cid`: collection id
  - `name`: collection name
  - `operations`: number of operations following for the collection

  Note that only those collections will be included that the transaction actually
  modifies. Collections that are not affected or that are only read from during a
  transaction will not be included:

      {
        "tick":"247908333471119",
        "type":2200,
        "tid":"247908331963791",
        "collections": [
          {
            "cid":"247908330259855",
            "name":"UnitTestsReplication",
            "operations":1
          },
          {
            "cid":"247908330980751",
            "name":"UnitTestsReplication2",
            "operations":1
          }
        ]
      }

  Each transaction is logged as an uninterrupted sequence. All document operations 
  following a transaction start event should have their `tid` attribute set to the 
  current transaction's id. Successfully committed transactions are ended with a 
  `transaction commit` event.

  If a replication client encounters any other event type than document operations 
  or a `transaction commit` event, the transaction can be considered as being failed
  and should not be applied by the client. In any case, clients should buffer 
  transactional operations first, and apply them only if the transaction was committed
  successfully.

- 2201: multi-document transaction committed:
  
  This event is logged for all successfully committed transactions. The event
  attributes are the same as for the `transaction start` event.

      {
        "tick":"247908333864335",
        "type":2201,
        "tid":"247908331963791",
        "collections": [
          {
            "cid":"247908330259855",
            "name":"UnitTestsReplication",
            "operations":1
          },
          {
            "cid":"247908330980751",
            "name":"UnitTestsReplication2",
            "operations":1
          }
        ]
      }

- 2300: document insert/update operation

  This event is logged for document insertions and updates. The `cid` attribute
  contains the id of the collection the document is inserted/updated in. The `key`
  attribute contains the document key, the `rev` attribute contains the document
  revision id. 

  If the `tid` attribute is present, the document was inserted/updated as part of
  a transaction. The `tid` attribute then contains the transaction's id. If no `tid`
  attribute is present, then the document was inserted/updated in a standalone
  operation.
  
  If the event contains the `oldRev` attribute, then the operation was carried out
  as an update of an existing document. The `oldRev` attribute contains the 
  revision id of the replaced document. If the `oldRev` attribute is not present,
  the operation was carried out as an insert.

  The `data` attribute contains the actual document, with the usual `_key` and 
  `_rev` attributes plus all user-defined data.

  Standalone document operation:

      {
        "tick":"247908293100943",
        "type":2300,
        "cid":"247908289299855",
        "key":"abc",
        "rev":"247908292904335",
        "oldRev":"247908292117903",
        "data": {
          "_key":"abc",
          "_rev":"247908292904335",
          "test":4
        }
      }

  Transactional document operation:

      {
        "tick":"247908333602191",
        "type":2300,
        "tid":"247908331963791",
        "cid":"247908330259855",
        "key":"12345",
        "rev":"247908332160399",
        "data": {
          "_key":"12345",
          "_rev":"247908332160399",
          "test":1
        }
      }

- 2301: edge insert/update operation:

  This event is logged for edge inserts/updates. The structure is the same as for
  the document insert/update event. Additional, the `data` attribute will contain the
  sub-attributes `_from` and `_to`, which indicate the vertices connected by the edge.
  `_from` and `_to` will contain the vertex collection id plus the vertex document key:

      {
        "tick":"247908299916687",
        "type":2301,
        "cid":"247908298474895",
        "key":"abc",
        "rev":"247908299720079",
        "data": {
          "_key":"abc",
          "_rev":"247908299720079",
          "_from":"247908297753999/test1",
          "_to":"247908297753999/test2",
          "test":1
        }
      }

- 2302: document/edge deletion:

  This event is logged for deletions of documents and edges. The `cid` attribute contains the
  id of the collection the document/edge is deleted in. The `key` attribute indicates the
  key of the deleted document/edge. The `rev` attribute contains the revision id of the
  deletion, and the `oldRev` attribute contains the revision id of the deleted document/edge.

  There is no `data` attribute with more details about the deleted document/edge.
  
  If the `tid` attribute is present, the document/edge was deleted as part of a
  transaction. The `tid` attribute then contains the transaction's id. If no `tid`
  attribute is present, then the document/edge was delete in a standalone operation.

      {
        "tick":"247908303783311",
        "type":2302,
        "cid":"247908301882767",
        "key":"abc",
        "rev":"247908303586703",
        "oldRev":"247908302865807"
      }

Transactions{#RefManualReplicationTransactions}
-----------------------------------------------

Transactions are logged as an uninterrupted sequence, starting with a `transaction start` 
event, and finishing with a `transaction commit` event. Between these two events, all
transactional document/edge operations will be logged. 

All transactional document/edge operations will carry a `tid` attribute, which indicates
the transaction id. Non-transactional operations will not carry a `tid` attribute.

Replication clients can apply non-transactional operations directly. In constrast, they
should not apply transactional operations directly, but buffer them first and apply them
altogether when they encounter the `transaction commit` event for the transaction.

If a replication client encounters some unexpected event during a transaction (i.e. some
event that is neither a ocument/edge operation nor a `transaction commit` event), it 
should abort the ongoing transaction and discard all buffered operations. It can then
consider the current transaction as failed.
