////////////////////////////////////////////////////////////////////////////////////
//
// global variables
//
////////////////////////////////////////////////////////////////////////////////

var DEFAULT_QUERY_COLLECTION = "query";

////////////////////////////////////////////////////////////////////////////////
//
// Helper functions
//
////////////////////////////////////////////////////////////////////////////////

function throwIfError (requestResult) {
  
  if (requestResult == undefined) {    
    throw "Result is empty";
  }
  
  if (requestResult["error"] != undefined && requestResult["error"]) {
    if (requestResult["errorMessage"] == undefined) {
        throw "Unknown error";      
    }
    throw requestResult["errorMessage"];
  }  
}

function help () {
  print("--------------- HELP -----------------------------------------------");
  print("predefined objects:");
  print("  avocado:                               AvocadoConnection");
  print("  db:                                    AvocadoDatabase");
  print("examples:");
  print(" > db._collections();                    list all collections");
  print(" > db.<coll_name>.all();                 list all documents");
  print(" > id = db.<coll_name>.save({ ... });    save a document");
  print(" > db.<coll_name>.delete(<_id>);         delete a document");
  print(" > db.<coll_name>.document(<_id>);       get a document");
  print(" > helpQueries();                        query help");
  print(" > exit");  
  print("--------------------------------------------------------------------");
}

function helpQueries () {
  print("--------------- HELP -----------------------------------------------");
  print("create query template:");
  print(" > qt1 = db.createQueryTemplate(\"select ...\");     simple query");
  print(" > qt2 = db.createQueryTemplate(                   complex query");
  print("             {query:\"select...\",");
  print("              name:\"qname\",");
  print("              collection:\"q\"");
  print("              ... });");
  print(" > qt3 = db.getQueryTemplate(\"4334:2334\");         query by id");
  print(" > qt1.update(\"select ...\");                       update");                   
  print(" > qt1.delete(\"4334:2334\");                        delete");
  print("create query instance:");
  print(" > qi1 = qt1.getInstance();                        from tmplate");
  print(" > qi2 = db.createQueryInstance(\"select...\");      without template");
  print(" > qi2.bind(\"a\":\"b\")");
  print("execute query:");
  print(" > cu1 = qi1.execute();                            returns cursor");
  print("loop over all results:");
  print(" > while (cu1.hasNext()) { print( cu1.next() ); }");
  print("--------------------------------------------------------------------");
}

////////////////////////////////////////////////////////////////////////////////
//
// AvocadoCollection
//
////////////////////////////////////////////////////////////////////////////////

function AvocadoCollection (database, data) {
  this._database = database;

  if (typeof data === "string") {
    this.name = data;
  }
  else {
    for (var i in data) {
      this[i] = data[i];
    }
  }
}

AvocadoCollection.prototype.all = function () {
  var str = this._database._connection.get("/_api/documents/" + encodeURIComponent(this.name));

  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
    
  throwIfError(requestResult);

  if (requestResult["documents"] != undefined )  {
    return requestResult["documents"];    
  }
  return undefined;
}

AvocadoCollection.prototype.document = function (id) {
  var str = this._database._connection.get("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  throwIfError(requestResult);
  
  if (requestResult["document"] != undefined)  {
    return requestResult["document"];
  }
  return requestResult;
}

AvocadoCollection.prototype.save = function (data) {    
  var str = this._database._connection.post("/_api/document/" + encodeURIComponent(this.name), JSON.stringify(data));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
  throwIfError(requestResult);
  
  if (requestResult["_id"] != undefined)  {
    return requestResult["_id"];
  }
  return requestResult;
}

AvocadoCollection.prototype.delete = function (id) {    
  var str = this._database._connection.delete("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  throwIfError(requestResult);
  
  return true;
}

AvocadoCollection.prototype.update = function (id, data) {    
  var str = this._database._connection.put("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id), JSON.stringify(data));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  throwIfError(requestResult);

  return true;
}


////////////////////////////////////////////////////////////////////////////////
//
// QueryCursor
//
////////////////////////////////////////////////////////////////////////////////

function QueryCursor (database, data) {
  this._database = database;
  this.data = data;
  this._hasNext = false;
  this._hasMore = false;
  this._pos = 0;
  this._count = 0;
  this._total = 0;
  
  if (data.result != undefined) {
    this._count = data.result.length;
    
    if (this._pos < this._count) {
      this._hasNext = true;
    }
    
    if (data.hasMore != undefined && data.hasMore) {
      this._hasMore = true;
    }    
  }
}

QueryCursor.prototype.hasNext = function () {
  return this._hasNext;
}

QueryCursor.prototype.next = function () {
  if (!this._hasNext) {
      throw "No more results";
  }
  
  var result = this.data.result[this._pos];
  this._pos++;
    
  if (this._pos == this._count) {
    // reached last result
    
    this._hasNext = false;
    this._pos = 0;
    
    if (this._hasMore) {
      this._hasMore = false;
      
      // load more results      
      var str = this._database._connection.put("/_api/cursor/"+ encodeURIComponent(this.data._id),  "");

      var requestResult = undefined;
      if (str != undefined) {
        requestResult = JSON.parse(str);
      }
    
      throwIfError(requestResult);
      
      this.data = requestResult;
      this._count = requestResult.result.length;
    
      if (this._pos < this._count) {
        this._hasNext = true;
      }
    
      if (requestResult.hasMore != undefined && requestResult.hasMore) {
        this._hasMore = true;
      }                
    }    
  }
    
  return result;
}


////////////////////////////////////////////////////////////////////////////////
//
// QueryInstance
//
////////////////////////////////////////////////////////////////////////////////

function QueryInstance (database, data) {
  this._database = database;
  this.doCount = false;
  this.bindVars = {};
  
  if (typeof data === "string") {
    this.query = data;
  }
  else if (data instanceof QueryTemplate) {
    this.queryTemplate = data;
    if (data.document.query == undefined) {
      data.load();
      if (data.document.query == undefined) {
        throw "could not load query string";
      }
    }
    this.query = data.document.query;
  }   
}

QueryInstance.prototype.bind = function (key, value) {
  this.bindVars[key] = value;
}

QueryInstance.prototype.setCount = function (bool) {
  if (bool) {
    this.doCount = true;
  }
  else {
    this.doCount = false;    
  }
}

QueryInstance.prototype.execute = function () {
  var body = {
    query : this.query,
    count : this.doCount,
    bindVars : this.bindVars
  }
  
  var str = this._database._connection.post("/_api/cursor",  JSON.stringify(body));

  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
    
  throwIfError(requestResult);
  
  return new QueryCursor(this._database, requestResult);
}

////////////////////////////////////////////////////////////////////////////////
//
// QueryTemplate
//
////////////////////////////////////////////////////////////////////////////////

function QueryTemplate (database, data) {
  this._database = database;

  this.document = {
    collection : DEFAULT_QUERY_COLLECTION
  };
  
  if (typeof data === "string") {
    this.document.query = data;
  }
  else {    
    if (data["query"] != undefined) {
      this.document.query = data["query"];
    }

    if (data["_id"] != undefined) {
      this.document._id = data["_id"];
      this._id = this.document._id;
    }

    if (data["collection"] != undefined) {
      this.document.collection = data["collection"];
    }    

    if (data["name"] != undefined) {
      this.document.name = data["name"];
    }    

    // TODO additional attributes
  }
  
  if (this.document.query == undefined && this.document._id == undefined) {
    throw "QueryTemplate needs query or _id.";
  }
}

QueryTemplate.prototype.getInstance = function () {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }
  
  return new QueryInstance(this._database, this);
}

QueryTemplate.prototype.update = function (data) {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }
  
  if (typeof data === "string") {
    this.document.query = data;
  }
  else {
    // update query or name    
    if (data["query"] != undefined) {
      this.document.query = data["query"];
    }

    if (data["name"] != undefined) {
      this.document.name = data["name"];
    }    

    // TODO additional attributes
  }
  
  var queryCollection = new AvocadoCollection(this._database, this.document.collection);
  if (queryCollection.update(this.document._id, this.document)) {
    return true;
  }
  
  return false;
}

QueryTemplate.prototype.delete = function () {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }

  var queryCollection = new AvocadoCollection(this._database, this.document.collection);
  if (queryCollection.delete(this.document._id)) {
    this.document = {};
    return true;
  }
  
  return false;
}

QueryTemplate.prototype.load = function () {
  if (this.document._id == undefined) {
    throw "QueryTemplate needs _id for loading";    
  }

  var coll = this.document.collection;
  var queryCollection = new AvocadoCollection(this._database, coll);
  var requestResult = queryCollection.document(this.document._id);
    
  if (requestResult != undefined) {

    if (requestResult.query == undefined) {
      throw "document is not a query";    
    }
    // loaded
    this.document = requestResult;
    
    if (this.document.collection == undefined) {
      this.document.collection = coll;
    }
    
    return true;
  }
  return false;  

}

QueryTemplate.prototype.save = function () {
  if (this.document._id != undefined) {
    throw "use update to save changes";    
  }
  
  var queryCollection = new AvocadoCollection(this._database, this.document.collection);
  var requestResult = queryCollection.save(this.document);
    
  if (requestResult != undefined) {
    // document saved
    this.document._id = requestResult;
    this._id = this.document._id;
    return true;
  }
  
  return false;  
}


////////////////////////////////////////////////////////////////////////////////
//
// AvocadoDatabase
//
////////////////////////////////////////////////////////////////////////////////

function AvocadoDatabase (connection) {
  this._connection = connection;
  
  // defaults
  this.queryCollection = "query";
  
}

AvocadoDatabase.prototype._collections = function () {
  var str = this._connection.get("/_api/collections");
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
  throwIfError(requestResult);
    
  if (requestResult["collections"] != undefined) {
    
    // add all collentions to object
    for (var i in requestResult["collections"]) {
      this[i] = new AvocadoCollection(this, requestResult["collections"][i]);
    }
      
    return requestResult["collections"];
  }
  
  return undefined;
}

AvocadoDatabase.prototype._collection = function (id) {
  var str = this._connection.get("/_api/collection/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
  throwIfError(requestResult);
    
  if (requestResult["name"] != undefined) {
    
    this[requestResult["name"]] = new AvocadoCollection(this, requestResult);
      
    return requestResult;
  }
  
  return undefined;
}

AvocadoDatabase.prototype.createQueryTemplate = function (queryData) {  
  var qt = new QueryTemplate(this, queryData);
  if (qt.save()) {
    return qt;
  }
  
  return undefined;
}

AvocadoDatabase.prototype.createQueryInstance = function (queryData) {  
  return new QueryInstance(this, queryData);
}

AvocadoDatabase.prototype.getQueryTemplate = function (id) {  
  var qt = new QueryTemplate(this, {"_id" : id});
  if (qt.load()) {
    return qt;
  }
  
  return undefined;
}


try {

  //
  // default database
  // 
  db = new AvocadoDatabase(avocado);

  //
  // load collection data
  // 
  db._collections();

  help();
}
catch (err) {
  print(COLOR_RED + "connection failure: " + err + COLOR_BLACK);
}
