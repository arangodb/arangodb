!CHAPTER Explaining queries

If it is unclear how a given query will perform, clients can retrieve a query's execution plan 
from the AQL query optimizer without actually executing the query. Getting the query execution 
plan from the optimizer is called *explaining*.

An explain will throw an error if the given query is syntactically invalid. Otherwise, it will
return the execution plan and some information about what optimizations could be applied to
the query. The query will not be executed.

Explaining a query can be achieved by calling the [HTTP REST API](../../HTTP/AqlQuery/index.html).
A query can also be explained from the ArangoShell using `ArangoStatement`'s `explain` method.

By default, the query optimizer will return what it considers to be the *optimal plan*. The
optimal plan will be returned in the `plan` attribute of the result. If `explain` is
called with option `allPlans` set to `true`, all plans will be returned in the `plans`
attribute instead. The result object will also contain an attribute *warnings*, which 
is an array of warnings that occurred during optimization or execution plan creation.

Each plan in the result is an object with the following attributes:
- *nodes*: the array of execution nodes of the plan. The list of available node types
   can be found [here](Optimizer.md)
- *estimatedCost*: the total estimated cost for the plan. If there are multiple
  plans, the optimizer will choose the plan with the lowest total cost.
- *collections*: an array of collections used in the query
- *rules*: an array of rules the optimizer applied. The list of rules can be 
  found [here](Optimizer.md)
- *variables*: array of variables used in the query (note: this may contain
  internal variables created by the optimizer)

Here is an example for retrieving the execution plan of a simple query:

    @startDocuBlockInline 07_workWithAQL_statementsExplain
    @EXAMPLE_ARANGOSH_OUTPUT{07_workWithAQL_statementsExplain}
    |var stmt = db._createStatement(
     "FOR user IN _users RETURN user");
    stmt.explain();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 07_workWithAQL_statementsExplain

As the output of `explain` is very detailed, it is recommended to use some
scripting to make the output less verbose:

    @startDocuBlockInline 08_workWithAQL_statementsPlans
    @EXAMPLE_ARANGOSH_OUTPUT{08_workWithAQL_statementsPlans}
    |var formatPlan = function (plan) {
    |    return { estimatedCost: plan.estimatedCost,
    |        nodes: plan.nodes.map(function(node) {
                return node.type; }) }; };
    formatPlan(stmt.explain().plan);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 08_workWithAQL_statementsPlans

If a query contains bind parameters, they must be added to the statement **before**
`explain` is called:

    @startDocuBlockInline 09_workWithAQL_statementsPlansBind
    @EXAMPLE_ARANGOSH_OUTPUT{09_workWithAQL_statementsPlansBind}
    |var stmt = db._createStatement(
    | `FOR doc IN @@collection FILTER doc.user == @user RETURN doc`
    );
    stmt.bind({ "@collection" : "_users", "user" : "root" });
    stmt.explain();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 09_workWithAQL_statementsPlansBind

In some cases the AQL optimizer creates multiple plans for a single query. By default
only the plan with the lowest total estimated cost is kept, and the other plans are
discarded. To retrieve all plans the optimizer has generated, `explain` can be called
with the option `allPlans` set to `true`.

In the following example, the optimizer has created two plans:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer0
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer0}
    |var stmt = db._createStatement(
      "FOR user IN _users FILTER user.user == 'root' RETURN user");
    stmt.explain({ allPlans: true }).plans.length;
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer0

To see a slightly more compact version of the plan, the following transformation can be applied:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer1
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer1}
    ~var stmt = db._createStatement("FOR user IN _users FILTER user.user == 'root' RETURN user");
    |stmt.explain({ allPlans: true }).plans.map(
        function(plan) { return formatPlan(plan); });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer1

`explain` will also accept the following additional options:
- *maxPlans*: limits the maximum number of plans that are created by the AQL query optimizer
- *optimizer.rules*: an array of to-be-included or to-be-excluded optimizer rules
  can be put into this attribute, telling the optimizer to include or exclude
  specific rules. To disable a rule, prefix its name with a `-`, to enable a rule, prefix it
  with a `+`. There is also a pseudo-rule `all`, which will match all optimizer rules.

The following example disables all optimizer rules but `remove-redundant-calculations`:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer2
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer2}
    ~var stmt = db._createStatement("FOR user IN _users FILTER user.user == 'root' RETURN user");
    |stmt.explain({ optimizer: {
       rules: [ "-all", "+remove-redundant-calculations" ] } });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer2


The contents of an execution plan are meant to be machine-readable. To get a human-readable
version of a query's execution plan, the following commands can be used:

    @startDocuBlockInline 10_workWithAQL_statementsPlansOptimizer3
    @EXAMPLE_ARANGOSH_OUTPUT{10_workWithAQL_statementsPlansOptimizer3}
    var query = "FOR doc IN mycollection FILTER doc.value > 42 RETURN doc";
    require("@arangodb/aql/explainer").explain(query, {colors:false});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock 10_workWithAQL_statementsPlansOptimizer3

The above command prints the query's execution plan in the ArangoShell directly, focusing
on the most important information.


