
@startDocuBlock post_api_explain
@brief explain an AQL query and return information about it

@RESTHEADER{POST /_api/explain, Explain an AQL query, explainQuery}

A JSON object describing the query and query parameters.

@RESTBODYPARAM{query,string,required,string}
the query which you want explained; If the query references any bind variables,
these must also be passed in the attribute *bindVars*. Additional
options for the query can be passed in the *options* attribute.

@RESTBODYPARAM{bindVars,array,optional,object}
key/value pairs representing the bind parameters.

@RESTBODYPARAM{options,object,optional,explain_options}
Options for the query

@RESTSTRUCT{allPlans,explain_options,boolean,optional,}
if set to *true*, all possible execution plans will be returned.
The default is *false*, meaning only the optimal plan will be returned.

@RESTSTRUCT{maxNumberOfPlans,explain_options,integer,optional,int64}
an optional maximum number of plans that the optimizer is
allowed to generate. Setting this attribute to a low value allows to put a
cap on the amount of work the optimizer does.

@RESTSTRUCT{optimizer,explain_options,object,optional,explain_options_optimizer}
Options related to the query optimizer.

@RESTSTRUCT{rules,explain_options_optimizer,array,optional,string}
A list of to-be-included or to-be-excluded optimizer rules can be put into this
attribute, telling the optimizer to include or exclude specific rules. To disable
a rule, prefix its name with a `-`, to enable a rule, prefix it with a `+`. There is
also a pseudo-rule `all`, which matches all optimizer rules. `-all` disables all rules.

@RESTDESCRIPTION

To explain how an AQL query would be executed on the server, the query string
can be sent to the server via an HTTP POST request. The server will then validate
the query and create an execution plan for it. The execution plan will be
returned, but the query will not be executed.

The execution plan that is returned by the server can be used to estimate the
probable performance of the query. Though the actual performance will depend
on many different factors, the execution plan normally can provide some rough
estimates on the amount of work the server needs to do in order to actually run
the query.

By default, the explain operation will return the optimal plan as chosen by
the query optimizer The optimal plan is the plan with the lowest total estimated
cost. The plan will be returned in the attribute *plan* of the response object.
If the option *allPlans* is specified in the request, the result will contain
all plans created by the optimizer. The plans will then be returned in the
attribute *plans*.

The result will also contain an attribute *warnings*, which is an array of
warnings that occurred during optimization or execution plan creation. Additionally,
a *stats* attribute is contained in the result with some optimizer statistics.
If *allPlans* is set to *false*, the result will contain an attribute *cacheable*
that states whether the query results can be cached on the server if the query
result cache were used. The *cacheable* attribute is not present when *allPlans*
is set to *true*.

Each plan in the result is a JSON object with the following attributes:
- *nodes*: the array of execution nodes of the plan.

- *estimatedCost*: the total estimated cost for the plan. If there are multiple
  plans, the optimizer will choose the plan with the lowest total cost.

- *collections*: an array of collections used in the query

- *rules*: an array of rules the optimizer applied.

- *variables*: array of variables used in the query (note: this may contain
  internal variables created by the optimizer)

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the query is valid, the server will respond with *HTTP 200* and
return the optimal execution plan in the *plan* attribute of the response.
If option *allPlans* was set in the request, an array of plans will be returned
in the *allPlans* attribute instead.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request,
or if the query contains a parse error. The body of the response will
contain the error details embedded in a JSON object.
Omitting bind variables if the query references any will also result
in an *HTTP 400* error.

@RESTRETURNCODE{404}
The server will respond with *HTTP 404* in case a non-existing collection is
accessed in the query.

@EXAMPLES

Valid query

@EXAMPLE_ARANGOSH_RUN{RestExplainValid}
    var url = "/_api/explain";
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    for (var i = 0; i < 10; ++i) { db.products.save({ id: i }); }
    body = {
      query : "FOR p IN products RETURN p"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

A plan with some optimizer rules applied

@EXAMPLE_ARANGOSH_RUN{RestExplainOptimizerRules}
    var url = "/_api/explain";
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db.products.ensureSkiplist("id");
    for (var i = 0; i < 10; ++i) { db.products.save({ id: i }); }
    body = {
      query : "FOR p IN products LET a = p.id FILTER a == 4 LET name = p.name SORT p.id LIMIT 1 RETURN name",
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using some options

@EXAMPLE_ARANGOSH_RUN{RestExplainOptions}
    var url = "/_api/explain";
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db.products.ensureSkiplist("id");
    for (var i = 0; i < 10; ++i) { db.products.save({ id: i }); }
    body = {
      query : "FOR p IN products LET a = p.id FILTER a == 4 LET name = p.name SORT p.id LIMIT 1 RETURN name",
      options : {
        maxNumberOfPlans : 2,
        allPlans : true,
        optimizer : {
          rules: [ "-all", "+use-index-for-sort", "+use-index-range" ]
        }
      }
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Returning all plans

@EXAMPLE_ARANGOSH_RUN{RestExplainAllPlans}
    var url = "/_api/explain";
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    db.products.ensureHashIndex("id");
    body = {
      query : "FOR p IN products FILTER p.id == 25 RETURN p",
      options: {
        allPlans: true
      }
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

A query that produces a warning

@EXAMPLE_ARANGOSH_RUN{RestExplainWarning}
    var url = "/_api/explain";
    body = {
      query : "FOR i IN 1..10 RETURN 1 / 0"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Invalid query (missing bind parameter)

@EXAMPLE_ARANGOSH_RUN{RestExplainInvalid}
    var url = "/_api/explain";
    var cn = "products";
    db._drop(cn);
    db._create(cn);
    body = {
      query : "FOR p IN products FILTER p.id == @id LIMIT 2 RETURN p.n"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 400);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

The data returned in the **plan** attribute of the result contains one element per AQL top-level statement
(i.e. `FOR`, `RETURN`, `FILTER` etc.). If the query optimizer removed some unnecessary statements,
the result might also contain less elements than there were top-level statements in the AQL query.

The following example shows a query with a non-sensible filter condition that
the optimizer has removed so that there are less top-level statements.

@EXAMPLE_ARANGOSH_RUN{RestExplainEmpty}
    var url = "/_api/explain";
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    body = '{ "query" : "FOR i IN [ 1, 2, 3 ] FILTER 1 == 2 RETURN i" }';

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
