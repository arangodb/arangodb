Server-side db-Object implementation
------------------------------------

We [already talked about the arangosh db Object implementation](../GettingStarted/Arangosh.md), Now a little more about the server version, so the following examples won't work properly in arangosh.

Server-side methods of the *db object* will return an `[object ShapedJson]`. This datatype is a very lightweight JavaScript object that contains an internal pointer to where the document data are actually stored in memory or on disk. Especially this is not a fullblown copy of the document's complete data. 

When such an object's property is accessed, this will invoke an accessor function. For example, accessing `doc.name` of such an object will call a C++ function behind the scenes to fetch the actual value for the property `name`. When a property is written to this, it will also trigger an accessor function. This accessor function will first copy all property values into the object's own properties, and then discard the pointer to the data in memory. From this point on, the accessor functions will not do anything special, so the object will behave like a normal JavaScript object.

All of this is done for performance reasons. It often allows ommitting the creation of big JavaScript objects that contain lots of data. For example, if all thats needed from a document is a single property, fully constructing the document as a JavaScript object has a high overhead (CPU time for processing, memory, plus later V8 garbage collection).

Here's an example:
```js
var ShapedJson = require("@arangodb").ShapedJson;
// fetches document from collection into a JavaScript object
var doc = db.test.any(); 

// check if the document object is a shaped object
// returns true for shaped objects, false otherwise
doc instanceof ShapedJson; 

// invokes the property read accessor. returns property value byValue
doc.name; 

// invokes the property write accessor. will copy document data
// into the JavaScript object once
// and store the value in the property as requested
doc.name = "test"; 

// doc will now behave like a regular object 
doc.foo = "bar"; 
```

There is one gotcha though with such objects: the accessor functions are only invoked when accessing top level properties. When accessing nested properties, the accessor will only be called for the top level property, which will then return the requested property's value. Accessing a subproperty of this returned property however does not have a reference to the original object.

Here's an example:

```js
// create an object with a nested property
db.test.save({ name: { first: "Jan" } });
doc;
{ 
  "_id" : "test/2056013422404", 
  "_rev" : "2056013422404", 
  "_key" : "2056013422404", 
  "name" : { 
    "first" : "Jan" 
  } 
}

// now modify the nested property
doc.name.first = "test";
doc;
{ 
  "_id" : "test/2056013422404", 
  "_rev" : "2056013422404", 
  "_key" : "2056013422404", 
  "name" : { 
    "first" : "Jan"  /* oops */
  } 
}
```

So what happened here? The statement `doc.name.first = "test"` calls the read accessor for property `name` first. This produces an object `{"name":"Jan"}` whose property `first` is modifed directly afterwards. But the object `{"name":"Jan"}` is a temporary object and not the same (`===`) object as `doc.name`. This is why updating the nested property effectively failed.

There is no way to detect this in a read accessor unfortunately. It does not have any idea about what will be done with the returned object. So this case cannot be tracked/trapped in the accessor.

A workaround for this problem would be to clone the object in the user code if the document is going to be modified. This will make all modification safe. The cloning can also be made conditional for cases when the object is an instance of `ShapedJson` or when nested properties are to be accessed. Cloning is not required when the object is no `ShapedJson` or when only top level properties are accessed.

Only those documents that are stored in a collections datafile will be returned as `ShapedJson`. The documents, that are still in the write-ahead-log, will always be returned as regular JavaScript objects, as they are not yet shaped and the optimization would not work. However, when a document is transfered from the write-ahead-log to the collection's datafile cannot be safely predicted by an application, so the same document can be returned in one way or the other. The only safe way is to check if the object is an instance of `ShapedJson` as above.
