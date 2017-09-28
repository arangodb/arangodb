The AQL query optimizer
=======================

AQL queries are sent through an optimizer before execution. The task of the optimizer is
to create an initial execution plan for the query, look for optimization opportunities and
apply them. As a result, the optimizer might produce multiple execution plans for a
single query. It will then calculate the costs for all plans and pick the plan with the
lowest total cost. This resulting plan is considered to be the *optimal plan*, which is
then executed.

The optimizer is designed to only perform optimizations if they are *safe*, in the
meaning that an optimization should not modify the result of a query. A notable exception
to this is that the optimizer is allowed to change the order of results for queries that
do not explicitly specify how results should be sorted.

### Execution plans

The `explain` command can be used to query the optimal executed plan or even all plans
the optimizer has generated. Additionally, `explain` can reveal some more information
about the optimizer's view of the query.

### Inspecting plans using the explain helper

The `explain` method of `ArangoStatement` as shown in the next chapters creates very verbose output.
You can work on the output programmatically, or use this handsome tool that we created
to generate a more human readable representation.

You may use it like this: (we disable syntax highlighting here)

    @startDocuBlockInline AQLEXP_01_axplainer
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_01_axplainer}
    ~addIgnoreCollection("test")
    ~db._drop("test");
    db._create("test");
    for (i = 0; i < 100; ++i) { db.test.save({ value: i }); }
    db.test.ensureIndex({ type: "skiplist", fields: [ "value" ] });
    var explain = require("@arangodb/aql/explainer").explain;
    explain("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value", {colors:false});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_01_axplainer


### Execution plans in detail

Let's have a look at the raw json output of the same execution plan
using the `explain` method of `ArangoStatement`:

    @startDocuBlockInline AQLEXP_01_explainCreate
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_01_explainCreate}
    stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_01_explainCreate

As you can see, the result details are very verbose so we will not show them in full in the next
sections. Instead, let's take a closer look at the results step by step.

#### Execution nodes

In general, an execution plan can be considered to be a pipeline of processing steps.
Each processing step is carried out by a so-called *execution node*

The `nodes` attribute of the `explain` result contains these *execution nodes* in
the *execution plan*. The output is still very verbose, so here's a shorted form of it:

    @startDocuBlockInline AQLEXP_02_explainOverview
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_02_explainOverview}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain().plan.nodes.map(function (node) { return node.type; });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_02_explainOverview

*Note that the list of nodes might slightly change in future versions of ArangoDB if
new execution node types get added or the optimizer create somewhat more
optimized plans).*

When a plan is executed, the query execution engine will start with the node at
the bottom of the list (i.e. the *ReturnNode*).

The *ReturnNode*'s purpose is to return data to the caller. It does not produce
data itself, so it will ask the node above itself, this is the *CalculationNode*
in our example.
*CalculationNode*s are responsible for evaluating arbitrary expressions. In our
example query, the *CalculationNode* will evaluate the value of `i.value`, which
is needed by the *ReturnNode*. The calculation will be applied for all data the
*CalculationNode* gets from the node above it, in our example the *IndexNode*.

Finally, all of this needs to be done for documents of collection `test`. This is
where the *IndexNode* enters the game. It will use an index (thus its name)
to find certain documents in the collection and ship it down the pipeline in the
order required by `SORT i.value`. The *IndexNode* itself has a *SingletonNode*
as its input. The sole purpose of a *SingletonNode* node is to provide a single empty
document as input for other processing steps. It is always the end of the pipeline.

Here's a summary:
* SingletonNode: produces an empty document as input for other processing steps.
* IndexNode: iterates over the index on attribute `value` in collection `test`
  in the order required by `SORT i.value`.
* CalculationNode: evaluates the result of the calculation `i.value > 97` to `true` or `false`
* CalculationNode: calculates return value `i.value`
* ReturnNode: returns data to the caller


#### Optimizer rules

Note that in the example, the optimizer has optimized the `SORT` statement away.
It can do it safely because there is a sorted skiplist index on `i.value`, which it has
picked in the *IndexNode*. As the index values are iterated over in sorted order
anyway, the extra *SortNode* would have been redundant and was removed.

Additionally, the optimizer has done more work to generate an execution plan that
avoids as much expensive operations as possible. Here is the list of optimizer rules
that were applied to the plan:

    @startDocuBlockInline AQLEXP_03_explainRules
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_03_explainRules}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain().plan.rules;
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_03_explainRules

Here is the meaning of these rules in context of this query:
* `move-calculations-up`: moves a *CalculationNode* as far up in the processing pipeline
  as possible
* `move-filters-up`: moves a *FilterNode* as far up in the processing pipeline as
  possible
* `remove-redundant-calculations`: replaces references to variables with references to
  other variables that contain the exact same result. In the example query, `i.value`
  is calculated multiple times, but each calculation inside a loop iteration would
  produce the same value. Therefore, the expression result is shared by several nodes.
* `remove-unnecessary-calculations`: removes *CalculationNode*s whose result values are
  not used in the query. In the example this happens due to the `remove-redundant-calculations`
  rule having made some calculations unnecessary.
* `use-indexes`: use an index to iterate over a collection instead of performing a
  full collection scan. In the example case this makes sense, as the index can be
  used for filtering and sorting.
* `remove-filter-covered-by-index`: remove an unnessary filter whose functionality
  is already covered by an index. In this case the index only returns documents 
  matching the filter.
* `use-index-for-sort`: removes a `SORT` operation if it is already satisfied by
  traversing over a sorted index

Note that some rules may appear multiple times in the list, with number suffixes.
This is due to the same rule being applied multiple times, at different positions
in the optimizer pipeline.


#### Collections used in a query

The list of collections used in a plan (and query) is contained in the `collections`
attribute of a plan:

    @startDocuBlockInline AQLEXP_04_explainCollections
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_04_explainCollections}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain().plan.collections
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_04_explainCollections

The `name` attribute contains the name of the `collection`, and `type` is the
access type, which can be either `read` or `write`.


#### Variables used in a query

The optimizer will also return a list of variables used in a plan (and query). This
list will contain auxiliary variables created by the optimizer itself. This list
can be ignored by end users in most cases.


#### Cost of a query

For each plan the optimizer generates, it will calculate the total cost. The plan
with the lowest total cost is considered to be the optimal plan. Costs are
estimates only, as the actual execution costs are unknown to the optimizer.
Costs are calculated based on heuristics that are hard-coded into execution nodes.
Cost values do not have any unit.


### Retrieving all execution plans

To retrieve not just the optimal plan but a list of all plans the optimizer has
generated, set the option `allPlans` to `true`:

This will return a list of all plans in the `plans` attribute instead of in the
`plan` attribute:

    @startDocuBlockInline AQLEXP_05_explainAllPlans
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_05_explainAllPlans}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain({ allPlans: true });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_05_explainAllPlans

### Retrieving the plan as it was generated by the parser / lexer

To retrieve the plan which closely matches your query, you may turn off most
optimization rules (i.e. cluster rules cannot be disabled if you're running
the explain on a cluster coordinator) set the option `rules` to `-all`:

This will return an unoptimized plan in the `plan`:

    @startDocuBlockInline AQLEXP_06_explainUnoptimizedPlans
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_06_explainUnoptimizedPlans}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain({ optimizer: { rules: [ "-all" ] } });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_06_explainUnoptimizedPlans

Note that some optimizations are already done at parse time (i.e. evaluate simple constant
calculation as `1 + 1`)


### Turning specific optimizer rules off

Optimizer rules can also be turned on or off individually, using the `rules` attribute.
This can be used to enable or disable one or multiple rules. Rules that shall be enabled
need to be prefixed with a `+`, rules to be disabled should be prefixed with a `-`. The
pseudo-rule `all` matches all rules.

Rules specified in `rules` are evaluated from left to right, so the following works to
turn on just the one specific rule:

    @startDocuBlockInline AQLEXP_07_explainSingleRulePlans
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_07_explainSingleRulePlans}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain({ optimizer: { rules: [ "-all", "+use-index-range" ] } });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_07_explainSingleRulePlans

By default, all rules are turned on. To turn off just a few specific rules, use something
like this:

    @startDocuBlockInline AQLEXP_08_explainDisableSingleRulePlans
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_08_explainDisableSingleRulePlans}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain({ optimizer: { rules: [ "-use-index-range", "-use-index-for-sort" ] } });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_08_explainDisableSingleRulePlans

The maximum number of plans created by the optimizer can also be limited using the
`maxNumberOfPlans` attribute:

    @startDocuBlockInline AQLEXP_09_explainMaxNumberOfPlans
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_09_explainMaxNumberOfPlans}
    ~var stmt = db._createStatement("FOR i IN test FILTER i.value > 97 SORT i.value RETURN i.value");
    stmt.explain({ maxNumberOfPlans: 1 });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_09_explainMaxNumberOfPlans

### Optimizer statistics

The optimizer will return statistics as a part of an `explain` result.

The following attributes will be returned in the `stats` attribute of an `explain` result:

- `plansCreated`: total number of plans created by the optimizer
- `rulesExecuted`: number of rules executed (note: an executed rule does not
  indicate a plan was actually modified by a rule)
- `rulesSkipped`: number of rules skipped by the optimizer

### Warnings

For some queries, the optimizer may produce warnings. These will be returned in
the `warnings` attribute of the `explain` result:

    @startDocuBlockInline AQLEXP_10_explainWarn
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_10_explainWarn}
    var stmt = db._createStatement("FOR i IN 1..10 RETURN 1 / 0")
    stmt.explain().warnings;
    ~db._drop("test")
    ~removeIgnoreCollection("test")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_10_explainWarn

There is an upper bound on the number of warning a query may produce. If that
bound is reached, no further warnings will be returned.


### Things to consider for optimizing queries
While the optimizer can fix some things in queries, its not allowed to take some assumptions,
that you, the user, knowing what queries are intended to do can take. It may pull calculations
to the front of the execution, but it may not cross certain borders.

So in certain cases you may want to move calculations in your query, so they're cheaper.
Even more expensive is if you have calculacions that are executed in javascript:

    @startDocuBlockInline AQLEXP_11_explainjs
    @EXAMPLE_ARANGOSH_OUTPUT{AQLEXP_11_explainjs}
    db._explain('FOR x IN 1..10 LET then=DATE_NOW() FOR y IN 1..10 LET now=DATE_NOW() LET nowstr=CONCAT(now, x, y, then) RETURN nowstr', {}, {colors: false})
    db._explain('LET now=DATE_NOW() FOR x IN 1..10 FOR y IN 1..10 LET nowstr=CONCAT(now, x, y, now) RETURN nowstr', {}, {colors: false})
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock AQLEXP_11_explainjs

You can see, that the optimizer found `1..10` is specified twice, but can be done first one time.

While you may see time passing by during the execution of the query and its calls to `DATE_NOW()`
this may not be the desired thing in first place. The queries V8 Expressions will however also use
significant resources, since its executed 10 x 10 times => 100 times. Now if we don't care
for the time ticking by during the query execution, we may fetch the time once at the startup
of the query, which will then only give us one V8 expression at the very start of the query.

Next to bringing better performance, this also obeys the [DRY principle](https://en.wikipedia.org/wiki/Don't_repeat_yourself).

### Optimization in a cluster

When you're running AQL in the cluster, the parsing of the query is done on the
coordinator. The coordinator then chops the query into snipets, which are to
remain on the coordinator, and others that are to be distributed over the network
to the shards. The cutting sites are interconnected
via *Scatter-*, *Gather-* and *RemoteNodes*.

These nodes mark the network borders of the snippets. The optimizer strives to reduce the amount
of data transfered via these network interfaces by pushing `FILTER`s out to the shards,
as it is vital to the query performance to reduce that data amount to transfer over the
network links.

Snippets marked with **DBS** are executed on the shards, **COOR** ones are excuted on the coordinator.

**As usual, the optimizer can only take certain assumptions for granted when doing so,
i.e. [user-defined functions have to be executed on the coordinator](../Extending/README.md).
If in doubt, you should modify your query to reduce the number interconnections between your snippets.**

When optimizing your query you may want to look at simpler parts of it first.

### List of execution nodes

The following execution node types will appear in the output of `explain`:

* *SingletonNode*: the purpose of a *SingletonNode* is to produce an empty document
  that is used as input for other processing steps. Each execution plan will contain
  exactly one *SingletonNode* as its top node.
* *EnumerateCollectionNode*: enumeration over documents of a collection (given in
  its *collection* attribute) without using an index.
* *IndexNode*: enumeration over one or many indexes (given in its *indexes* attribute)
  of a collection. The index ranges are specified in the *condition* attribute of the node.
* *EnumerateListNode*: enumeration over a list of (non-collection) values.
* *FilterNode*: only lets values pass that satisfy a filter condition. Will appear once
  per *FILTER* statement.
* *LimitNode*: limits the number of results passed to other processing steps. Will
  appear once per *LIMIT* statement.
* *CalculationNode*: evaluates an expression. The expression result may be used by
  other nodes, e.g. *FilterNode*, *EnumerateListNode*, *SortNode* etc.
* *SubqueryNode*: executes a subquery.
* *SortNode*: performs a sort of its input values.
* *AggregateNode*: aggregates its input and produces new output variables. This will
  appear once per *COLLECT* statement.
* *ReturnNode*: returns data to the caller. Will appear in each read-only query at
  least once. Subqueries will also contain *ReturnNode*s.
* *InsertNode*: inserts documents into a collection (given in its *collection*
  attribute). Will appear exactly once in a query that contains an *INSERT* statement.
* *RemoveNode*: removes documents from a collection (given in its *collection*
  attribute). Will appear exactly once in a query that contains a *REMOVE* statement.
* *ReplaceNode*: replaces documents in a collection (given in its *collection*
  attribute). Will appear exactly once in a query that contains a *REPLACE* statement.
* *UpdateNode*: updates documents in a collection (given in its *collection*
  attribute). Will appear exactly once in a query that contains an *UPDATE* statement.
* *UpsertNode*: upserts documents in a collection (given in its *collection*
  attribute). Will appear exactly once in a query that contains an *UPSERT* statement.
* *NoResultsNode*: will be inserted if *FILTER* statements turn out to be never
  satisfiable. The *NoResultsNode* will pass an empty result set into the processing
  pipeline.

For queries in the cluster, the following nodes may appear in execution plans:

* *ScatterNode*: used on a coordinator to fan-out data to one or multiple shards.
* *GatherNode*: used on a coordinator to aggregate results from one or many shards
  into a combined stream of results.
* *DistributeNode*: used on a coordinator to fan-out data to one or multiple shards,
  taking into account a collection's shard key.
* *RemoteNode*: a *RemoteNode* will perform communication with another ArangoDB
  instances in the cluster. For example, the cluster coordinator will need to
  communicate with other servers to fetch the actual data from the shards. It
  will do so via *RemoteNode*s. The data servers themselves might again pull
  further data from the coordinator, and thus might also employ *RemoteNode*s.
  So, all of the above cluster relevant nodes will be accompanied by a *RemoteNode*.


### List of optimizer rules

The following optimizer rules may appear in the `rules` attribute of a plan:

* `move-calculations-up`: will appear if a *CalculationNode* was moved up in a plan.
  The intention of this rule is to move calculations up in the processing pipeline
  as far as possible (ideally out of enumerations) so they are not executed in loops
  if not required. It is also quite common that this rule enables further optimizations
  to kick in.
* `move-filters-up`: will appear if a *FilterNode* was moved up in a plan. The
  intention of this rule is to move filters up in the processing pipeline as far
  as possible (ideally out of inner loops) so they filter results as early as possible.
* `sort-in-values`: will appear when the values used as right-hand side of an `IN`
  operator will be pre-sorted using an extra function call. Pre-sorting the comparison
  array allows using a binary search in-list lookup with a logarithmic complexity instead
  of the default linear complexity in-list lookup.
* `remove-unnecessary-filters`: will appear if a *FilterNode* was removed or replaced.
  *FilterNode*s whose filter condition will always evaluate to *true* will be
  removed from the plan, whereas *FilterNode* that will never let any results pass
  will be replaced with a *NoResultsNode*.
* `remove-redundant-calculations`: will appear if redundant calculations (expressions
  with the exact same result) were found in the query. The optimizer rule will then
  replace references to the redundant expressions with a single reference, allowing
  other optimizer rules to remove the then-unneeded *CalculationNode*s.
* `remove-unnecessary-calculations`: will appear if *CalculationNode*s were removed
  from the query. The rule will removed all calculations whose result is not
  referenced in the query (note that this may be a consequence of applying other
  optimizations).
* `remove-redundant-sorts`: will appear if multiple *SORT* statements can be merged
  into fewer sorts.
* `interchange-adjacent-enumerations`: will appear if a query contains multiple
  *FOR* statements whose order were permuted. Permutation of *FOR* statements is
  performed because it may enable further optimizations by other rules.
* `remove-collect-variables`: will appear if an *INTO* clause was removed from a *COLLECT*
  statement because the result of *INTO* is not used. May also appear if a result
  of a *COLLECT* statement's *AGGREGATE* variables is not used.
* `propagate-constant-attributes`: will appear when a constant value was inserted
  into a filter condition, replacing a dynamic attribute value.
* `replace-or-with-in`: will appear if multiple *OR*-combined equality conditions
  on the same variable or attribute were replaced with an *IN* condition.
* `remove-redundant-or`: will appear if multiple *OR* conditions for the same variable
  or attribute were combined into a single condition.
* `use-indexes`: will appear when an index is used to iterate over a collection.
  As a consequence, an *EnumerateCollectionNode* was replaced with an
  *IndexNode* in the plan.
* `remove-filter-covered-by-index`: will appear if a *FilterNode* was removed or replaced
  because the filter condition is already covered by an *IndexNode*.
* `remove-filter-covered-by-traversal`: will appear if a *FilterNode* was removed or replaced
  because the filter condition is already covered by an *TraversalNode*.
* `use-index-for-sort`: will appear if an index can be used to avoid a *SORT*
  operation. If the rule was applied, a *SortNode* was removed from the plan.
* `move-calculations-down`: will appear if a *CalculationNode* was moved down in a plan.
  The intention of this rule is to move calculations down in the processing pipeline
  as far as possible (below *FILTER*, *LIMIT* and *SUBQUERY* nodes) so they are executed
  as late as possible and not before their results are required.
* `patch-update-statements`: will appear if an *UpdateNode* was patched to not buffer
  its input completely, but to process it in smaller batches. The rule will fire for an
  *UPDATE* query that is fed by a full collection scan, and that does not use any other
  indexes and subqueries.
* `optimize-traversals`: will appear if either the edge or path output variable in an
  AQL traversal was optimized away, or if a *FILTER* condition from the query was moved
  in the *TraversalNode* for early pruning of results.
* `inline-subqueries`: will appear when a subquery was pulled out in its surrounding scope,
  e.g. `FOR x IN (FOR y IN collection FILTER y.value >= 5 RETURN y.test) RETURN x.a`
  would become `FOR tmp IN collection FILTER tmp.value >= 5 LET x = tmp.test RETURN x.a`
* `geo-index-optimizer`: will appear when a geo index is utilized.
* `remove-sort-rand`: will appear when a *SORT RAND()* expression is removed by
  moving the random iteration into an *EnumerateCollectionNode*. This optimizer rule
  is specific for the MMFiles storage engine.
* `reduce-extraction-to-projection`: will appear when an *EnumerationCollectionNode* that
  would have extracted an entire document was modified to return only a projection of each
  document. This optimizer rule is specific for the RocksDB storage engine.

The following optimizer rules may appear in the `rules` attribute of cluster plans:

* `distribute-in-cluster`: will appear when query parts get distributed in a cluster.
  This is not an optimization rule, and it cannot be turned off.
* `scatter-in-cluster`: will appear when scatter, gather, and remote nodes are inserted
  into a distributed query. This is not an optimization rule, and it cannot be turned off.
* `distribute-filtercalc-to-cluster`: will appear when filters are moved up in a
  distributed execution plan. Filters are moved as far up in the plan as possible to
  make result sets as small as possible as early as possible.
* `distribute-sort-to-cluster`: will appear if sorts are moved up in a distributed query.
  Sorts are moved as far up in the plan as possible to make result sets as small as possible
  as early as possible.
* `remove-unnecessary-remote-scatter`: will appear if a RemoteNode is followed by a
  ScatterNode, and the ScatterNode is only followed by calculations or the SingletonNode.
  In this case, there is no need to distribute the calculation, and it will be handled
  centrally.
* `undistribute-remove-after-enum-coll`: will appear if a RemoveNode can be pushed into
  the same query part that enumerates over the documents of a collection. This saves
  inter-cluster roundtrips between the EnumerateCollectionNode and the RemoveNode.

Note that some rules may appear multiple times in the list, with number suffixes.
This is due to the same rule being applied multiple times, at different positions
in the optimizer pipeline.
