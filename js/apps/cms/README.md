aye_aye
=======

This is a TODO-MVC Foxx-Application written for ArangoDB.
One objective of Foxx-Apps implemented in [ArangoDB](https://github.com/triAGENS/ArangoDB) is to create simple REST-Interfaces easily.
In addition an asset pipeline is given that can easily deliver single page applications (f.e. using [backbone.js]{http://www.backbonejs.org}).
So a Foxx-App can for instance be used to provide a basic application logic and a persistent data-storage in the ArangoDB.
And this exactly what the aye_aye does.
It consists of two major parts the Frontend and the Backend.


# Frontend

The backbone-frontend is taken from [addyosmani/todomvc](https://github.com/addyosmani/todomvc/tree/gh-pages/architecture-examples/backbone/).
All credits to the authors.
The objective of the frontend is to show how the MVC pattern is applied in backbone.

However the original frontend used backbone-local-storage in order to persist the data.
This local-storage has been replaced to use the REST-Interface defined in the aye_aye-foxx.
The required modifications for this goal were quite trivial:
Add an `url` function to all models and collections and remove the backbone-local-storage.


# Backend

The backend is a Foxx-Application written for [ArangoDB](https://github.com/triAGENS/ArangoDB).
It consists of five parts:

## Manifest


The manifest gives general description of the Foxx: Name and Version.
Also it defines the positions of all files, including the assets to build the frontend.

## Setup

This script is executed whenever the aye_aye is mounted to a path.
It creates the collection for our ToDos.

## Teardown

This script is executed whenever a the aye_aye is uninstalled from a path.
It drops the collection of ToDos.

## Models

Define the models that are executed within the Arango.
The `todos` model offers 4 functions:

* save: Store a new Todo.
* destroy: Remove the given Todo.
* list: Give a list of all Todos.
* update: Overwrite the given Todo.

## Libs

Further libraries, but this feature is not used in the aye_aye.

## The App

The `ayeaye.js` defines the REST interface.
At first it requires the `todo`-model and creates a new Foxx.
Then it defines the offered REST functions:

* /todo GET: Invokes todos.list
* /todo POST: Invokes todos.save
* /todo/:id PUT: Invokes todos.update
* /todo/:id DELETE: Invokes todo.destroy