# How to use ArangoDB with Sails.js

First install the [Sails.js](http://sailsjs.org) framework using NPM:

```
npm install -g sails
```

Now you can create a new Sails.js app named `somename` with the following command:

```
sails new somename
```

Now we need to add ArangoDB to this new application. First `cd` into the freshly created `somename` directory. Then install the ArangoDB adapter for Sails.js with the following command:

```
npm install sails-arangodb
```

This however only installs the necessary dependency. We need to configure the application to load the adapter. Open the file `config/connections.js`. You will see a list of example configurations for possible connections to databases. Remove the ones you don't need (or just keep all of them), and add the following configuration (adjust the host, port, database name and graph name to your needs):

```js
localArangoDB: {
  adapter: 'sails-arangodb',
  host: 'localhost',
  port: 8529,
  database: {
    name: 'sails',
    graph: 'sails'
  }
}
```

Now, you can configure your app to use the ArangoDB as your default connection. You do this by adjusting the file `config/models.js`:

```js
module.exports.models = {
  connection: 'localArangoDB' // this is the name from the connections.js file
  // ...
};
```

Your app is now configured to use ArangoDB for all models by default. You can now for example create a blueprint controller by typing the following in your console:

```
sails generate api todos
```

Now you can boot your application with:

```
sails lift
```

You can now access `http://localhost:1337/todos` and see an empty list of todos. And then create a todo by visiting `localhost:1337/todos/create?name=john`. This will create the according document (that has an attribute `name` with the value `john`) in the todos collection of the selected database. You will also see the document when you visit `http://localhost:1337/todos` again.

**Author**: [Lucas Dohmen](https://github.com/moonglum)

**Tags**: #nodejs
