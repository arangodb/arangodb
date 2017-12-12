# Writing multi-line AQL queries

## Problem

I want to write an AQL query that spans multiple lines in my JavaScript source code, 
but it does not work. How to do this?

## Solution

AQL supports multi-line queries, and the AQL editor in ArangoDB's web interface supports
them too.

When issued programmatically, multi-line queries can be a source of errors, at least in
some languages. For example, JavaScript is notoriously bad at handling multi-line (JavaScript) 
statements, and until recently it had no support for multi-line strings.

In JavaScript, there are three ways of writing a multi-line AQL query in the source code:

* string concatenation
* ES6 template strings
* query builder

Which method works best depends on a few factors, but is often enough a simple matter of preference.
Before deciding on any, please make sure to read the recipe for [avoiding parameter injection](AvoidingInjection.md) 
too. 

### String concatenation

We want the query `FOR doc IN collection FILTER doc.value == @what RETURN doc` to become
more legible in the source code. 
 
Simply splitting the query string into three lines will leave us with a parse error in
JavaScript:

```js
/* will not work */
var query = "FOR doc IN collection
             FILTER doc.value == @what
             RETURN doc"; 
```

Instead, we could do this:

```js
var query = "FOR doc IN collection " +
            "FILTER doc.value == @what " + 
            "RETURN doc"; 
```

This is perfectly valid JavaScript, but it's error-prone. People have spent ages on finding 
subtle bugs in their queries because they missed a single whitespace character at the
beginning or start of some line.

Please note that when assembling queries via string concatenation, you should still use
bind parameters (as done above with `@what`) and not insert user input values into the
query string without sanitation.

### ES6 template strings

ES6 template strings are easier to get right and also look more elegant. They can be used
inside ArangoDB since version 2.5. but some other platforms don't support them et.
For example, they can't be used in IE and older node.js versions. So use them if your 
environment supports them and your code does not need to run on any non-ES6 environments.

Here's the query string declared via an ES6 template string (note that the string must
be enclosed in backticks now):

```js
var query = `FOR doc IN collection 
             FILTER doc.value == @what 
             RETURN doc`; 
```
The whitespace in the template string-variant is much easier to get right than when doing 
the string concatenation.

There are a few things to note regarding template strings:

* ES6 template strings can be used to inject JavaScript values into the string dynamically.
  Substitutions start with the character sequence `${`. Care must be taken if this sequence
  itself is used inside the AQL query string (currently this would be invalid AQL, but this
  may change in future ArangoDB versions). Additionally, any values injected into the query
  string using parameter substitutions will not be escaped correctly automatically, so again
  special care must be taken when using this method to keep queries safe from parameter 
  injection.

* a multi-line template string will actually contain newline characters. This is not necessarily
  the case when doing string concatenation. In the string concatenation example, we used 
  three lines of source code to create a single-line query string. We could have inserted 
  newlines into the query string there too, but we didn't. Just to point out that the two
  variants will not create bytewise-identical query strings. 

Please note that when using ES6 template strings for your queries, you should still use
bind parameters (as done above with `@what`) and not insert user input values into the
query string without sanitation.

There is a convenience function `aql` which can be used to safely
and easily build an AQL query with substitutions from arbitrary JavaScript values and
expressions. It can be invoked like this:

```js
const aql = require("@arangodb").aql; // not needed in arangosh

var what = "some input value";
var query = aql`FOR doc IN collection 
                     FILTER doc.value == ${what} 
                     RETURN doc`; 
```

The template string variant that uses `aql` is both convenient and safe. Internally, it
will turn the substituted values into bind parameters. The query string and the bind parameter
values will be returned separately, so the result of `query` above will be something like:

```js 
{ 
  "query" : "FOR doc IN collection FILTER doc.value == @value0 RETURN doc", 
  "bindVars" : { 
    "value0" : "some input value" 
  } 
}
```

### Query builder

ArangoDB comes bundled with a query builder named [aqb](https://www.npmjs.com/package/aqb).
That query builder can be used to programmatically construct AQL queries, without having
to write query strings at all.

Here's an example of its usage:

```js
var qb = require("aqb");

var jobs = db._createStatement({    
  query: (    
    qb.for('job').in('_jobs')    
    .filter(    
      qb('pending').eq('job.status')    
      .and(qb.ref('@queue').eq('job.queue'))    
      .and(qb.ref('@now').gte('job.delayUntil'))    
    )    
    .sort('job.delayUntil', 'ASC')    
    .limit('@max')    
    .return('job')    
  ),    
  bindVars: {    
    queue: queue._key,    
    now: Date.now(),    
    max: queue.maxWorkers - numBusy    
  }    
}).execute().toArray();   
```

As can be seen, aqb provides a fluent API that allows chaining function calls for
creating the individual query operations. This has a few advantages:

* flexibility: there is no query string in the source code, so the code can be formatted 
  as desired without having to bother about strings
* validation: the query can be validated syntactically by aqb before being actually executed 
  by the server. Testing of queries also becomes easier. Additionally, some IDEs may
  provide auto-completion to some extend and thus aid development
* security: built-in separation of query operations (e.g. `FOR`, `FILTER`, `SORT`, `LIMIT`) 
  and dynamic values (e.g. user input values)

aqb can be used inside ArangoDB and from node.js and even from within browsers.

**Authors**: [Jan Steemann](https://github.com/jsteemann)

**Tags**: #aql #aqb #es6
