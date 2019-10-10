---
layout: default
description: Queries don't necessarily have to involve collections.
title: AQL Queries Without Collections
---
Queries without collections
===========================

AQL queries typically access one or more collections to read from documents
or to modify them. Queries don't necessarily have to involve collections
however. Below are a few examples of that.

Following is a query that returns a string value. The result string is contained in an array
because the result of every valid query is an array:

{% aqlexample examplevar="examplevar" type="type" query="query" bind="bind" result="result" %}
@startDocuBlockInline aqlWithoutCollections_1
@EXAMPLE_AQL{aqlWithoutCollections_1}
RETURN "this will be returned"
@END_EXAMPLE_AQL
@endDocuBlock aqlWithoutCollections_1
{% endaqlexample %}
{% include aqlexample.html id=examplevar query=query bind=bind result=result %}

You may use variables, call functions and return arbitrarily structured results:

{% aqlexample examplevar="examplevar" type="type" query="query" bind="bind" result="result" %}
@startDocuBlockInline aqlWithoutCollections_2
@EXAMPLE_AQL{aqlWithoutCollections_2}
LET array = [1, 2, 3, 4]
RETURN { array, sum: SUM(array) }
@END_EXAMPLE_AQL
@endDocuBlock aqlWithoutCollections_2
{% endaqlexample %}
{% include aqlexample.html id=examplevar query=query bind=bind result=result %}

Language constructs such as the FOR loop can be used too. Below query
creates the Cartesian product of two arrays and concatenates the value pairs:

{% aqlexample examplevar="examplevar" type="type" query="query" bind="bind" result="result" %}
@startDocuBlockInline aqlWithoutCollections_3
@EXAMPLE_AQL{aqlWithoutCollections_3}
FOR year IN [ 2011, 2012, 2013 ]
  FOR quarter IN [ 1, 2, 3, 4 ]
    RETURN {
      year,
      quarter,
      formatted: CONCAT(quarter, " / ", year)
    }
@END_EXAMPLE_AQL
@endDocuBlock aqlWithoutCollections_3
{% endaqlexample %}
{% include aqlexample.html id=examplevar query=query bind=bind result=result %}
