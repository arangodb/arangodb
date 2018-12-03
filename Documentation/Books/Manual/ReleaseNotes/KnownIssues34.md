Known Issues
============

The following known issues are present in this version of ArangoDB and will be fixed
in follow-up releases:

Modifing documents in cluster using AQL and an incorrect custom shard key
-------------------------------------------------------------------------

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

* ArangoSearch index format in 3.4RC3 is incompatible to earlier issued release candidates
* ArangoSearch index format in 3.4RC4 is incompatible to earlier issued release candidates
* ArangoSearch ignores `_id` attribute even if `includeAllFields` is set to `true` (internal #445)
* Using score functions (BM25/TFIDF) in ArangoDB expression is not supported (internal #316)
* Using a loop variable in expressions within a corresponding SEARCH condition is not supported (internal #318)
* Score values evaluated by corresponding score functions (BM25/TFIDF) may differ in single-server and cluster with a collection having more than 1 shard (internal #508)
