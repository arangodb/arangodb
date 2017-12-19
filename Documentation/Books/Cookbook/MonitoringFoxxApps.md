#Monitoring your Foxx applications

**Note**: this recipe is working with ArangoDB 2.5 Foxx

##Problem
How to integrate a Foxx application into a monitoring system using the `collectd curl_JSON` plugin.

##Solution
Since Foxx native tongue is JSON, integrating it with the [collectd curl_JSON plugin](MonitoringWithCollectd.html)
should be an easy exercise.
We have a Foxx-Application which can receive Data and write it into a collection. We specify an easy input Model:

```javascript
Model = Foxx.Model.extend({
  schema: {
    // Describe the attributes with Joi here
    '_key': Joi.string(),
    'value': Joi.number()
  }
});
```

And use a simple Foxx-Route to inject data into our collection:

```javascript
/** Creates a new FirstCollection
 *
 * Creates a new FirstCollection-Item. The information has to be in the
 * requestBody.
 */
controller.post('/firstCollection', function (req, res) {
  var firstCollection = req.params('firstCollection');
  firstCollection.attributes.Date = Date.now();
  res.json(FirstCollection_repo.save(firstCollection).forClient());
}).bodyParam('firstCollection', {
  description: 'The FirstCollection you want to create',
  type: FirstCollection
});
```

Which we may do using `cURL`:

```javascript
echo '{"value":1 ,"_key":"13"}' | \
  curl -d @-  http://localhost:8529/_db/_system/collectable_foxx/data/firstCollection/firstCollection
```

We'd expect the value to be in the range of 1 to 5. Maybe the source of this data is a web-poll or something similar.

We now add another Foxx-route which we want to link with `collectd`:

```javascript
/**
 * we use a group-by construct to get the values:
 */
var db = require('org/arangodb').db;
var searchQuery = 'FOR x IN @@collection FILTER x.Date >= @until collect value=x.value with count into counter RETURN {[[CONCAT("choice", value)] : counter }';
controller.get('/firstCollection/lastSeconds/:nSeconds', function (req, res) {
  var until = Date.now() - req.params('nSeconds') * 1000;
  res.json(
    db._query(searchQuery, {
      '@collection': FirstCollection_repo.collection.name(),
      'until': until
    }).toArray()
  );
}).pathParam('nSeconds', {
  description: 'look up to n Seconds into the past',
  type: joi.string().required()
});
```

We inspect the return document using curl and [jq](http://stedolan.github.io/jq/) for nice formatting:

```javascript
curl 'http://localhost:8529/_db/_system/collectable_foxx/data/firstCollection/firstCollection/lastSeconds/10' |jq "."

[
  {
    "1": 3
    "3": 7
  }
]
```

We have to design the return values in a way that collectd's config syntax can simply grab it. This Route returns an object with flat key values where keys may range from 0 to 5.
We create a simple collectd configuration in `/etc/collectd/collectd.conf.d/foxx_simple.conf` that matches our API:

```javascript
# Load the plug-in:
LoadPlugin curl_json
# we need to use our own types to generate individual names for our gauges:
TypesDB "/etc/collectd/collectd.conf.d/foxx_simple_types.db"
<Plugin curl_json>
  # Adjust the URL so collectd can reach your arangod:
  <URL "http://localhost:8529/_db/_system/collectable_foxx/data/firstCollection/firstCollection/lastSeconds/10">
   # Set your authentication to Aardvark here:
   # User "foo"
   # Password "bar"
    <Key "choice0">
       Type "the_values"
     </Key>
    <Key "choice1">
       Type "first_values"
     </Key>
    <Key "choice2">
       Type "second_values"
     </Key>
    <Key "choice3">
       Type "third_values"
     </Key>
    <Key "choice4">
       Type "fourth_values"
     </Key>
    <Key "choice5">
       Type "fifth_values"
     </Key>
  </URL>
</Plugin>
```

To get nice metric names, we specify our own `types.db` file in `/etc/collectd/collectd.conf.d/foxx_simple_types.db`:

```
the_values    value:GAUGE:U:U
first_values    value:GAUGE:U:U
second_values    value:GAUGE:U:U
third_values    value:GAUGE:U:U
fourth_values    value:GAUGE:U:U
fifth_values    value:GAUGE:U:U
```

**Author:** [Wilfried Goesgens](https://github.com/dothebart)

**Tags:** #monitoring #foxx #json
