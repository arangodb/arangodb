# My first FoxxGenerator App

## Problem

I want to create an API with FoxxGenerator. As an example I want to use the statechart that was described in a [blog series](https://www.arangodb.com/2014/12/08/building-hypermedia-apis-foxxgenerator). How do I get started?

## Solution

In order to create a FoxxGenerator app, we first have to create the according folder structure. Create a folder `foxx_apps` somewhere where you have write access. Create a folder `databases` inside of it, and one called `_system` inside of that. Inside of this folder, create a folder for your app. Let's call it `books`. From now on we will work in that folder. We now create a file called `manifest.json` where we will add some meta information (if you want to get more information about the manifest file, check out [the documentation](https://docs.arangodb.com/Foxx/Develop/Manifest.html)):

```json
{
  "name": "books",
  "version": "0.0.1",
  "description": "Example on the blog",
  "author": "Lucas Dohmen",
  "license": "Apache 2 License",

  "controllers": {
    "/api": "books.js"
  },

  "rootElement": true
}
```

The `rootElement` is important, because the Siren standard (which the generated app will follow) uses root elements in JSON. In controllers we give the path to the file where we describe our statechart with the FoxxGenerator DSL. Let's create this file:

```js
(function () {
  'use strict';
  var FoxxGenerator = require('foxx_generator').Generator,
    Joi = require('joi'),
    generator;

  generator = new FoxxGenerator('books', {
    mediaType: 'application/vnd.siren+json',
    applicationContext: applicationContext,
  });

  // Transition definitions

  // Description of statechart

  generator.addStartState({
    transitions: [
    ]
  });

  generator.generate();
}());
```

Now you can start ArangoDB with the following command:

```
arangod --javascript.dev-app-path /PATH/TO/foxx_apps /PATH/TO/DB
```

If you now go to the app in the [admin interface](http://localhost:8529/_db/_system/_admin/aardvark/standalone.html#applications), you will see that it already created the first endpoint for you: The start state. From this point on you can follow the description in the [blogposts](https://www.arangodb.com/2014/12/08/building-hypermedia-apis-foxxgenerator) or describe your own statechart.

**Author**: [Lucas Dohmen](https://github.com/moonglum)

**Tags**: #foxx #foxxgenerator
