A Hello World Example
---------------------

The client API or browser sends a HTTP request to the ArangoDB server and the
server returns a HTTP response to the client. A HTTP request consists of a
method, normally *GET* or *POST* when using a browser, and a request path like
*/hello/world*. For a real Web server there are a zillion of other thing to
consider, we will ignore this for the moment. The HTTP response contains a
content type, describing how to interpret the returned data, and the data
itself.

In the following example, we want to define an action in ArangoDB, so that the
server returns the HTML document

```html
<html>
  <body>
   Hello World
  </body>
</html>
```

if asked *GET /hello/world*.

The server needs to know what function to call or what document to deliver if it
receives a request. This is called routing. All the routing information of
ArangoDB is stored in a collection *_routing*. Each entry in this collections
describes how to deal with a particular request path.

For the above example, add the following document to the _routing collection:

    @startDocuBlockInline HTML_01_routingCreateHtml
    @EXAMPLE_ARANGOSH_OUTPUT{HTML_01_routingCreateHtml}
    |db._routing.save({ 
    |  url: { 
    |    match: "/hello/world" 
    |  },
    |  content: { 
    |    contentType: "text/html", 
    |    body: "<html><body>Hello World</body></html>" 
    |  }
    });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock HTML_01_routingCreateHtml

In order to activate the new routing, you must either restart the server or call
the internal reload function.

    @startDocuBlockInline HTML_02_routingReload
    @EXAMPLE_ARANGOSH_OUTPUT{HTML_02_routingReload}
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock HTML_02_routingReload

Now use the browser and access http:// localhost:8529/hello/world

You should see the *Hello World* in our browser:

    @startDocuBlockInline HTML_03_routingCurlHtml
    @EXAMPLE_ARANGOSH_RUN{HTML_03_routingCurlHtml}
    var url = "/hello/world";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logRawResponse(response);
    db._query("FOR route IN _routing FILTER route.url.match == '/hello/world' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock HTML_03_routingCurlHtml


Matching a URL
--------------

There are a lot of options for the *url* attribute. If you define different
routing for the same path, then the following simple rule is applied in order to
determine which match wins: If there are two matches, then the more specific
wins. I. e, if there is a wildcard match and an exact match, the exact match is
preferred. If there is a short and a long match, the longer match wins.

### Exact Match

If the definition is

    { 
      url: { 
        match: "/hello/world" 
      } 
    }

then the match must be exact. Only the request for */hello/world* will match,
everything else, e. g. */hello/world/my* or */hello/world2*, will not match.

The following definition is a short-cut for an exact match.

    { 
      url: "/hello/world" 
    }


**Note**: While the two definitions will result in the same URL
matching, there is a subtle difference between them:

The former definition (defining *url* as an object with a *match* attribute)
will result in the URL being accessible via all supported HTTP methods (e.g.
*GET*, *POST*, *PUT*, *DELETE*, ...), whereas the latter definition (providing a string
*url* attribute) will result in the URL being accessible via HTTP *GET* and 
HTTP *HEAD* only, with all other HTTP methods being disabled. Calling a URL
with an unsupported or disabled HTTP method will result in an HTTP 501 
(not implemented) error.

### Prefix Match

If the definition is

    { 
      url: { 
        match: "/hello/world/*" 
      } 
    }

then the match can be a prefix match. The requests for */hello/world*,
*/hello/world/my*, and */hello/world/how/are/you* will all match. However
*/hello/world2* does not match. Prefix matches within a URL part,
i. e. */hello/world**, are not allowed. The wildcard must occur at the end,
i. e.

    /hello/*/world 

is also disallowed.

If you define two routes

    { url: { match: "/hello/world/*" } }
    { url: { match: "/hello/world/emil" } }

then the second route will be used for */hello/world/emil* because it is more
specific.

### Parameterized Match

A parameterized match is similar to a prefix match, but the parameters are also
allowed inside the URL path.

If the definition is

    { 
      url: { 
        match: "/hello/:name/world" 
      } 
    }

then the URL must have three parts, the first part being *hello* and the third
part *world*. For example, */hello/emil/world* will match, while
*/hello/emil/meyer/world* will not.

### Constraint Match

A constraint match is similar to a parameterized match, but the parameters can
carry constraints.

If the definition is

    { 
      url: { 
        match: "/hello/:name/world", 
        constraint: { 
          name: "/[a-z]+/" 
        } 
      }
    }

then the URL must have three parts, the first part being *hello* and the third
part *world*. The second part must be all lowercase.

It is possible to use more then one constraint for the same URL part.

    { 
      url: { 
        match: "/hello/:name|:id/world",
        constraint: { 
          name: "/[a-z]+/", id: "/[0-9]+/" 
        } 
      }
    }

### Optional Match

An optional match is similar to a parameterized match, but the last parameter is
optional.

If the definition is

    { 
      url: { 
        match: "/hello/:name?", 
        constraint: { 
          name: "/[a-z]+/"
        } 
      }
    }

then the URL */hello* and */hello/emil* will match.

If the definitions are

    { url: { match: "/hello/world" } }
    { url: { match: "/hello/:name", constraint: { name: "/[a-z]+/" } } }
    { url: { match: "/hello/*" } }

then the URL */hello/world* will be matched by the first route, because it is
the most specific. The URL */hello/you* will be matched by the second route,
because it is more specific than the prefix match.

### Method Restriction

You can restrict the match to specific HTTP methods.

If the definition is

    { 
      url: { 
        match: "/hello/world", 
        methods: [ "post", "put" ] 
      }
    }

then only HTTP *POST* and *PUT* requests will match.
Calling with a different HTTP method will result in an HTTP 501 error.

Please note that if *url* is defined as a simple string, then only the
HTTP methods *GET* and *HEAD* will be allowed, an all other methods will be
disabled:

    { 
      url: "/hello/world" 
    }

### More on Matching

Remember that the more specific match wins. 

- A match without parameter or wildcard is more specific than a match with
  parameters or wildcard.
- A match with parameter is more specific than a match with a wildcard.
- If there is more than one parameter, specificity is applied from left to
  right.

Consider the following definitions

    @startDocuBlockInline HTML_04_routingCreateMultiPath
    @EXAMPLE_ARANGOSH_OUTPUT{HTML_04_routingCreateMultiPath}
    |db._routing.save({ 
    |  url: { match: "/hello/world" },
       content: { contentType: "text/plain", body: "Match No 1"} });
    |db._routing.save({ 
    |  url: { match: "/hello/:name", constraint: { name: "/[a-z]+/" } },
       content: { contentType: "text/plain", body: "Match No 2"} });
    |db._routing.save({ 
    |  url: { match: "/:something/world" },
       content: { contentType: "text/plain", body: "Match No 3"} });
    |db._routing.save({ 
    |  url: { match: "/hi/*" },
       content: { contentType: "text/plain", body: "Match No 4"} });
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock HTML_04_routingCreateMultiPath

Then

    @startDocuBlockInline HTML_05_routingGetMultiPath
    @EXAMPLE_ARANGOSH_RUN{HTML_05_routingGetMultiPath}
    | var url = ["/hello/world",
    | "/hello/emil",
    | "/your/world",
      "/hi/you"];
    | for (let i = 0; i < 4; i++) {
    |   var response = logCurlRequest('GET', url[i]);
    |   assert(response.code === 200);
    |   assert(parseInt(response.body.substr(-1)) === (i + 1))
    |   logRawResponse(response);
    }
    db._query("FOR route IN _routing FILTER route.content.contentType == 'text/plain' REMOVE route in _routing")
    require("internal").reloadRouting()
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock HTML_05_routingGetMultiPath


You can write the following document into the *_routing* collection
to test the above examples.

    {
      routes: [
    { url: { match: "/hello/world" }, content: "route 1" },
    { url: { match: "/hello/:name|:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "route 2" },
    { url: { match: "/:something/world" }, content: "route 3" },
    { url: { match: "/hello/*" }, content: "route 4" },
      ]
    }
