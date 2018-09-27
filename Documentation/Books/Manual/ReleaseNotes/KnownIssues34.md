Known Issues
============

The following known issues are present in this version of ArangoDB and will be fixed
in follow-up releases:

Installer
---------

* Rolling upgrades are not yet supported with the RC version of ArangoDB 3.4, when
  there are multiple instances of the same ArangoDB version running on the same host,
  and only some of them shall be upgraded, or they should be upgraded one after the
  other.

* Ubuntu 14.04 is not yet supported


Modify documents in cluster using AQL and an incorrect custom shard key
-----------------------------------------------------------------------

* In a very uncommon edge case there is an issue with an optimization rule in the cluster.

  If you are running a cluster and use a custom shard key on a collection (default is `_key`)
  **and** you provide a wrong shard key in a modifying query (`UPDATE`, `REPLACE`, `DELETE`)
  **and** the wrong shard key is on a different shard than the correct one, you'll get a
  `DOCUMENT NOT FOUND` error, instead of a modification.

  The modification always happens if the rule is switched off.

  Example query:

      UPDATE { _key: "123", shardKey: "wrongKey"} WITH { foo: "bar" } IN mycollection

  If your setup could run into this issue you may want to
  [deactivate the optimizing rule](../../AQL/ExecutionAndPerformance/Optimizer.html#turning-specific-optimizer-rules-off)
  `restrict-to-single-shard`.

More details can be found in [issue 6399](https://github.com/arangodb/arangodb/issues/6399).


ArangoSearch
------------

* ArangoSearch ignores `_id` attribute even if `includeAllFields` is set to `true` (internal #445)
* Creation of ArangoSearch on a large collection may cause OOM (internal #407)
* Long-running transaction with a huge number of DML operations may cause OOM (internal #407)
* Using score functions (BM25/TFIDF) in ArangoDB expression is not supported (internal #316)
* Using a loop variable in expressions within a corresponding SEARCH condition is not supported (internal #318)
* Data of "NONE" collection could be accessed via a view for a used regardless of rights check (internal #453)
* "NONE" read permission is not checked for a single link in a view with multiple links where others are "RW" for a user in cluster (internal #452)
* ArangoSearch doesn't support joins with satellite collections (internal #440)
* RocksDB recovery fails sometimes after renaming a view (internal #469)
* Link to a collection is not added to a view if it was already added to other view (internal #480)
* View directory remains in FS after a view was deleted (internal #485)
* Immediate deletion (right after creation) of a view with a link to one collection and indexed data reports failure but removes the link (internal #486)
* Segmentation fault happens when trying to delete a link in a view that has improper (via internal #486) state (internal #487)
