# Populating an autocomplete textbox

## Problem

I want to populate an autocomplete textbox with values from a collection. The completions
should adjust dynamically based on user input.

## Solution

Use a web framework for the client-side autocomplete rendering and event processing. Use
a collection with a (sorted) skiplist index and a range query on it to efficiently fetch
the completion values dynamically. Connect the two using a simple Foxx route.

### Install an example app

[This app](https://github.com/jsteemann/autocomplete) contains a jquery-powered web page
with an autocomplete textbox. It uses [jquery autocomplete](http://jqueryui.com/autocomplete/),
but every other web framework will also do.

The app can be installed as follows:

* in the ArangoDB web interface, switch into the **Applications** tab
* there, click *Add Application*
* switch on the *Github* tab
* for *Repository*, enter `jsteemann/autocomplete`
* for *Version*, enter `master`
* click *Install*

Now enter a mountpoint for the application. This is the URL path under which the
application will become available. For the example app, the mountpoint does not matter.
The web page in the example app assumes it is served by ArangoDB, too. So it uses a
relative URL `autocomplete`. This is easiest to set up, but in reality you might want
to have your web page served by a different server. In this case, your web page will
have to call the app mountpoint you just entered.

To see the example app in action, click on **Open**. The autocomplete textbox should be
populated with server data when at least two letters are entered.

### Backend code, setup script

The app also contains a backend route `/autocomplete` which is called by the web page to
fetch completions based on user input. The HTML code for the web page is 
[here](https://github.com/jsteemann/autocomplete/blob/master/assets/index.html).

Contained in the app is a [setup script](https://github.com/jsteemann/autocomplete/blob/master/scripts/setup.js)
that will create a collection named `completions` and load some initial data into it. The
example app provides autocompletion for US city names, and the setup script populates the
collection with about 10K city names.

The setup script also [creates a skiplist index on the lookup attribute](https://github.com/jsteemann/autocomplete/blob/master/scripts/setup.js#L10561),
so this attribute can be used for efficient filtering and sorting later.
The `lookup` attribute contains the city names already lower-cased, and the original 
(*pretty*) names are stored in attribute `pretty`. This attribute will be returned to
users.

### Backend code, Foxx route controller

The app contains a [controller](https://github.com/jsteemann/autocomplete/blob/master/demo.js).
The backend action `/autocomplete` that is called by the web page is also contained herein:

```js
controller.get("/autocomplete", function (req, res) {
  // search phrase entered by user
  var searchString = req.params("q").trim() || "";
  // lower bound for search range
  var begin = searchString.replace(/[^a-zA-Z]/g, " ").toLowerCase();
  if (begin.length === 0) {
    // search phrase is empty - no need to perfom a search at all
    res.json([]);
    return;
  }
  // upper bound for search range
  var end = begin.substr(0, begin.length - 1) + String.fromCharCode(begin.charCodeAt(begin.length - 1) + 1);
  // bind parameters for query
  var queryParams = {
    "@collection" : "completions",
    "begin" : begin,
    "end" : end
  };
  // the search query
  var query = "FOR doc IN @@collection FILTER doc.lookup >= @begin && doc.lookup < @end SORT doc.lookup RETURN { label: doc.pretty, value: doc.pretty, id: doc._key }";
  res.json(db._query(query, queryParams).toArray());
}
```

The backend code first fetches the search string from the URL parameter `q`. This is what the
web page will send us. 

Based on the search string, a lookup range is calculated. First of all, the search string is
lower-cased and all non-letter characters are removed from it. The resulting string is the
lower bound for the lookup. For the upper bound, we can use the lower bound with its last 
letter character code increased by one.

For example, if the user entered `Los A` into the textbox, the web page will send us the string
`Los A` in URL parameter `q`. Lower-casing and removing non-letter characters from the string,
we'll get `losa`. This is the lower bound. The upper bound is `losa`, with its last letter adjusted
to `b` (i.e. `losb`).

Finally, the lower and upper bounds are inserted into the following query using bind parameters
`@begin` and `@end`:

```
FOR doc IN @@collection 
  FILTER doc.lookup >= @begin && doc.lookup < @end 
  SORT doc.lookup 
  RETURN { 
    label: doc.pretty, 
    value: doc.pretty, 
    id: doc._key 
  }
```

The city names in the lookup range will be returned sorted. For each city, three values are
returned (the `id` contains the document key, the other two values are for display purposes).
Other frameworks may require a different return format, but that can easily be done by
adjusting the AQL query.

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #aql #autocomplete #jquery
