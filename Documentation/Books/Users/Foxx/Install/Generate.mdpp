!CHAPTER Generate a new Application

In this chapter we will make use of the Foxx manager as described [before](README.md).
This time we do not have anything to start from and would like ArangoDB to create a template for us.
For a generated application you only need to be prepared to answer some questions for meta information, don't worry if you do not yet have an answer, all of this info is optional and can be inserted later:

* Your name (e.g. `Author`)
* The name of your application (e.g. `MyApp`)
* A description what your application should do (e.g. `An application to access ArangoDB`)
* The license you want your application to have (e.g. `Apache 2`)
* Names of collections you only want this application to access (e.g. `["MyCollection"]`)

Now you can create a new empty application with just one line:

```
    unix> foxx-manager install EMPTY /example '{"author": "Author", "name": "MyApp", "description": "An application to access ArangoDB", "license": "Apache 2", "collectionNames": ["MyCollection"]}'
    Application MyApp version 0.0.1 installed successfully at mount point /example
    options used: {"author":"Author","name":"MyApp","description":"An application to access ArangoDB","license":"Apache 2","collectionNames":["MyCollection"],"configuration":{} }
```

This action now has generated you a fresh application in version 0.0.1.
It has set up all meta-information you have provided
For each collection in `collectionNames` it has created a collection and generated all 5 CRUD routes: List all, Create one, Read one, Update one, Delete one.

You can use the generator in all functions of the Foxx-manager that allows to install Foxx applications:

**install**

```
unix> foxx-manager install EMPTY /example '{"author": "Author", "name": "MyApp", "description": "An application to access ArangoDB", "license": "Apache 2", "collectionNames": ["MyCollection"]}'
Application MyApp version 0.0.1 installed successfully at mount point /example
options used: {"author":"Author","name":"MyApp","description":"An application to access ArangoDB","license":"Apache 2","collectionNames":["MyCollection"],"configuration":{} }
```

**replace**

```
unix> foxx-manager replace EMPTY /example '{"author": "Author", "name": "MyApp", "description": "An application to access ArangoDB", "license": "Apache 2", "collectionNames": ["MyCollection"]}'
Application MyApp version 0.0.1 installed successfully at mount point /example
options used: {"author":"Author","name":"MyApp","description":"An application to access ArangoDB","license":"Apache 2","collectionNames":["MyCollection"],"configuration":{} }
```

**upgrade**

```
unix> foxx-manager upgrade EMPTY /example '{"author": "Author", "name": "MyApp", "description": "An application to access ArangoDB", "license": "Apache 2", "collectionNames": ["MyCollection"]}'
Application MyApp version 0.0.1 installed successfully at mount point /example
options used: {"author":"Author","name":"MyApp","description":"An application to access ArangoDB","license":"Apache 2","collectionNames":["MyCollection"],"configuration":{} }
```
