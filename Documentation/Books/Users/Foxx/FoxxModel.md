<a name="details_on_foxxmodel"></a>
# Details on FoxxModel

The model doesn't know anything about the database. It is just a representation 
of the data as an JavaScript object.  You can add and overwrite the methods of 
the prototype in your model prototype via the object you give to extend. In 
your model file, export the model as `model`.

    var Foxx = require("org/arangodb/foxx");
    
    var TodoModel = Foxx.Model.extend({
    });
    
    exports.model = TodoModel;

A Foxx Model can be initialized with an object of attributes and their values.

There's also the possibility of annotation: The second hash you give to the
extend method are properties of the prototype. You can define an attributes 
property there:

    var Foxx = require("org/arangodb/foxx");
    
    var PersonModel = Foxx.Model.extend({
      // Your instance properties
    }, {
      // Your prototype properties
      attributes: {
        name: { type: "string", required: true },
        age: { type: "integer" },
        active: { type: "boolean", defaultValue: true }
    });
    
    exports.model = TodoModel;

This has two effects: On the one hand it provides documentation. If you annotated
your model, you can use it in the `bodyParam` method for documentation.
On the other hand it will influence the behavior of the constructor: If you provide
an object to the constructor, it will only take those attributes that are listed
in the attributes object. This is especially useful if you want to to initialize
the Model from user input. On the other hand it will set the default value for all
attributes that have not been set by hand. An example:

    var person = new PersonModel({
      name: "Pete",
      admin: true
    });

    person.attributes // => { name: "Pete", active: true }

<a name="extend"></a>
#### Extend

`FoxxModel::extend(instanceProperties, classProperties)`

Extend the Model prototype to add or overwrite methods. The first object contains the properties to be defined on the instance, the second object those to be defined on the prototype.

<a name="initialize"></a>
#### Initialize

`new FoxxModel(data)`

If you initialize a model, you can give it initial data as an object.

*Examples*

	instance = new Model({
		a: 1
	});

<a name="get"></a>
#### Get

`FoxxModel::get(name)`

Get the value of an attribute

*Examples*

	instance = new Model({
		a: 1
	});
	instance.get("a");

<a name="set"></a>
#### Set

`FoxxModel::set(name, value)`

Set the value of an attribute or multiple attributes at once

*Examples*

	instance = new Model({
		a: 1
	});
	instance.set("a", 2);
	instance.set({
		b: 2
	});

<a name="has"></a>
#### Has

`FoxxModel::has(name)`

Returns true if the attribute is set to a non-null or non-undefined value.

*Examples*

	instance = new Model({
	a: 1
	});
	instance.has("a"); //=> true
	instance.has("b"); //=> false

<a name="attributes"></a>
#### Attributes

<a name="fordb"></a>
#### forDB

`FoxxModel::forDB()`

Return a copy of the model which can be saved into ArangoDB

<a name="forclient"></a>
#### forClient

`FoxxModel::forClient()`

Return a copy of the model which you can send to the client.



<!--
### Extend

@copydetails JSF_foxx_model_extend

### Initialize

@copydetails JSF_foxx_model_initializer

### Get

@copydetails JSF_foxx_model_get

### Set

@copydetails JSF_foxx_model_set

### Has

@copydetails JSF_foxx_model_has

### Attributes

@copydetails JSF_foxx_model_attributes

### forDB

@copydetails JSF_foxx_model_forDB

### forClient

@copydetails JSF_foxx_model_forClient

-->