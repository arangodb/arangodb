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

function isErrorResult (requestResult) {

  if (requestResult == undefined) {    
    requestResult = {
      "error" : true,
      "code"  : 0,
      "errorNum" : 0,
      "errorMessage" : "Unknown error. Request result is empty"
    }    
  }
  
  if (requestResult["error"] != undefined && requestResult["error"]) {    
    var code     = requestResult["code"];
    var errorNum     = requestResult["errorNum"];
    var errorMessage = requestResult["errorMessage"];

    if ( typeof(COLOR_BRIGHT) != "undefined" ) {
      internal.output(COLOR_BRIGHT);
      internal.output("Error: ");
      internal.output(COLOR_OUTPUT_RESET);
    }
    else  {
      internal.output("Error: ");      
    }

    internal.output("["); 
    internal.output(code); 
    internal.output(":"); 
    internal.output(errorNum); 
    internal.output("] "); 
    print(errorMessage);
    
    return true;
  }  
  return false;
}

function getHeadline(text) {
  var x = parseInt((78 - text.length) / 2);
  
  var p = "";
  for (var i = 0; i < x; ++i) {
    p += "-";
  }
  
  if ( typeof(COLOR_BRIGHT) != "undefined" ) {
     return COLOR_BRIGHT + p + " " + text + " " + p + COLOR_OUTPUT_RESET + "\n";
  }
  else {
     return p + " " + text + " " + p + "\n";
  }
}


var HELP = 
getHeadline("Help") +
'Predefined objects:                                                 ' + "\n" +
'  avocado:                               AvocadoConnection          ' + "\n" +
'  db:                                    AvocadoDatabase            ' + "\n" +
'Example:                                                            ' + "\n" +
' > db._collections();                    list all collections       ' + "\n" +
' > db.<coll_name>.all();                 list all documents         ' + "\n" +
' > id = db.<coll_name>.save({ ... });    save a document            ' + "\n" +
' > db.<coll_name>.delete(<_id>);         delete a document          ' + "\n" +
' > db.<coll_name>.document(<_id>);       get a document             ' + "\n" +
' > help                                  show help pages            ' + "\n" +
' > helpQueries                           query help                 ' + "\n" +
' > exit                                                             ';

var helpQueries = 
getHeadline("Simple queries help") +
'Create query template:                                              ' + "\n" +
' > qt1 = db.createQueryTemplate("select ...");     simple query     ' + "\n" +
' > qt2 = db.createQueryTemplate(                   complex query    ' + "\n" +
'             {query:"select...",                                    ' + "\n" +
'              name:"qname",                                         ' + "\n" +
'              collection:"q"                                        ' + "\n" +
'              ... }                                                 ' + "\n" +
' > qt3 = db.getQueryTemplate("4334:2334");         query by id      ' + "\n" +
' > qt1.update("select ...");                       update           ' + "\n" +
' > qt1.delete("4334:2334");                        delete           ' + "\n" +
'Create query instance:                                              ' + "\n" +
' > qi1 = qt1.getInstance();                        from tmplate     ' + "\n" +
' > qi2 = db.createQueryInstance("select...");      without template ' + "\n" +
' > qi2.bind("key", "value");                                        ' + "\n" +
'Execute query:                                                      ' + "\n" +
' > cu1 = qi1.execute();                            returns cursor   ' + "\n" +
'Get all elements:                                                   ' + "\n" +
' > el = cu1.elements();                                             ' + "\n" +
'or loop over all results:                                           ' + "\n" +
' > while (cu1.hasNext()) { print( cu1.next() ); }                   ';

var helpAvocadoDatabase = 
getHeadline("AvocadoDatabase help") +
'AvocadoDatabase constructor:                                        ' + "\n" +
' > db2 = new AvocadoDatabase(connection);                           ' + "\n" +
'Functions:                                                          ' + "\n" +
'  _collections();                list all collections               ' + "\n" +
'                                 returns: list of AvocadoCollection ' + "\n" +
'  _collection(<name>);           get collection by name             ' + "\n" +
'                                 returns: AvocadoCollection         ' + "\n" +
'  createQueryTemplate(<data>);   create and return query template   ' + "\n" +
'                                 returns: AvocadoQueryTemplate      ' + "\n" +
'  getQueryTemplate(<id>);        get query template by id           ' + "\n" +
'                                 returns: dvocadoQueryTemplate      ' + "\n" +
'  createQueryInstance(<data>);   create and return query instance   ' + "\n" +
'                                 returns: AvocadoQueryInstance      ' + "\n" +
'  _help();                       this help                          ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  <collection names>                                                ';

var helpAvocadoCollection = 
getHeadline("AvocadoCollection help") +
'AvocadoCollection constructor:                                      ' + "\n" +
' > col = db.mycoll;                                                 ' + "\n" +
'Functions:                                                          ' + "\n" +
'  save(<data>);                   create document and return id     ' + "\n" +
'  document(<id>);                 get document by id                ' + "\n" +
'  update(<id>, <new data>);       over writes document by id        ' + "\n" +
'  delete(<id>);                   deletes document by id            ' + "\n" +
'  _help();                        this help                         ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                       database object                   ' + "\n" +
'  _id                             collection id                     ' + "\n" +
'  name                            collection name                   ' + "\n" +
'  status                          status id                         ' + "\n" +
'  figures                                                           ';

var helpAvocadoQueryCursor = 
getHeadline("AvocadoQueryCursor help") +
'AvocadoQueryCursor constructor:                                     ' + "\n" +
' > cu1 = qi1.execute();                                             ' + "\n" +
'Functions:                                                          ' + "\n" +
'  hasMore();                            returns true if there       ' + "\n" +
'                                        are more results            ' + "\n" +
'  next();                               returns the next document   ' + "\n" +
'  elements();                           returns all documents       ' + "\n" +
'  _help();                              this help                   ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                             database object             ' + "\n" +
'Example:                                                            ' + "\n" +
' > qi2 = db.createQueryInstance("select a from colA a");            ' + "\n" +
' > cu1 = qi2.execute();                                             ' + "\n" +
' > documents = cu1.elements();                                      ' + "\n" +
' > cu1 = qi2.execute();                                             ' + "\n" +
' > while (cu1.hasNext()) { print( cu1.next() ); }                   ';

var helpAvocadoQueryInstance = 
getHeadline("AvocadoQueryInstance help") +
'AvocadoQueryInstance constructor:                                   ' + "\n" +
' > qi1 = qt1.getInstance();                                         ' + "\n" +
' > qi2 = db.createQueryInstance("select ....");                     ' + "\n" +
'Functions:                                                          ' + "\n" +
'  bind(<key>, <value>);                 bind vars                   ' + "\n" +
'  setCount(true);                                                   ' + "\n" +
'  setMax(<max>);                                                    ' + "\n" +
'  execute();                            execute query and return    ' + "\n" +
'                                        query cursor                ' + "\n" +
'  _help();                              this help                   ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                             database object             ' + "\n" +
'  bindVars                              bind vars                   ' + "\n" +
'  doCount                               count results in server     ' + "\n" +
'  maxResults                            maximum number of results   ' + "\n" +
'  query                                 the query string            ' + "\n" +
'Example:                                                            ' + "\n" +
' > qi2 = db.createQueryInstance("select a from colA a               ' + "\n" +
'                             where a.x = @a@ and a.y = @b@");       ' + "\n" +
' > qi2.bind("a", "hello");                                          ' + "\n" +
' > qi2.bind("b", "world");                                          ' + "\n" +
' > cu1 = qi1.execute();                                             ';

var helpAvocadoQueryTemplate = 
getHeadline("AvocadoQueryTemplate help") +
'AvocadoQueryTemplate constructor:                                   ' + "\n" +
' > qt1 = db.createQueryTemplate("select ...");     simple query     ' + "\n" +
' > qt2 = db.createQueryTemplate(                   complex query    ' + "\n" +
'             {query:"select...",                                    ' + "\n" +
'              name:"qname",                                         ' + "\n" +
'              collection:"q"                                        ' + "\n" +
'              ... }                                                 ' + "\n" +
'Functions:                                                          ' + "\n" +
'  update(<new data>);                  update query template        ' + "\n" +
'  delete(<id>);                        delete query template by id  ' + "\n" +
'  getInstance();                       get a query instance         ' + "\n" +
'                                       returns: AvocadoQueryInstance' + "\n" +
'  _help();                             this help                    ' + "\n" +
'Attributes:                                                         ' + "\n" +
'  _database                            database object              ' + "\n" +
'  _id                                  template id                  ' + "\n" +
'  name                                 collection name              ' + "\n" +
'Example:                                                            ' + "\n" +
' > qt1 = db.getQueryTemplate("4334:2334");                          ' + "\n" +
' > qt1.update("select a from collA a");                             ' + "\n" +
' > qi1 = qt1.getInstance();                                         ' + "\n" +
' > qt1.delete("4334:2334");                                         ';

function help () {
  print(HELP);
  print(helpQueries);
  print(helpAvocadoDatabase);
  print(helpAvocadoCollection);
  print(helpAvocadoQueryTemplate);
  print(helpAvocadoQueryInstance);
  print(helpAvocadoQueryCursor);
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
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["documents"];    
}

AvocadoCollection.prototype.document = function (id) {
  var str = this._database._connection.get("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["document"];
}

AvocadoCollection.prototype.save = function (data) {    
  var str = this._database._connection.post("/_api/document/" + encodeURIComponent(this.name), JSON.stringify(data));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return requestResult["_id"];
}

AvocadoCollection.prototype.delete = function (id) {    
  var str = this._database._connection.delete("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  return !isErrorResult(requestResult);
}

AvocadoCollection.prototype.update = function (id, data) {    
  var str = this._database._connection.put("/_api/document/" + encodeURIComponent(this.name) + "/" + encodeURIComponent(id), JSON.stringify(data));
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }

  return !isErrorResult(requestResult);
}

AvocadoCollection.prototype._help = function () {  
  print(helpAvocadoCollection);
}

AvocadoCollection.prototype.toString = function () {  
  result  = "[object AvocadoCollection]";
  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// AvocadoQueryCursor
//
////////////////////////////////////////////////////////////////////////////////

function AvocadoQueryCursor (database, data) {
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

AvocadoQueryCursor.prototype.hasNext = function () {
  return this._hasNext;
}

AvocadoQueryCursor.prototype.next = function () {
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
    
      if (isErrorResult(requestResult)) {
        return undefined;
      }
      
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

AvocadoQueryCursor.prototype.elements = function () {  
  var result = [];
  
  while (this.hasNext()) { 
    result.push( this.next() ); 
  }
  
  return result;
}

AvocadoQueryCursor.prototype._help = function () {
  print(helpAvocadoQueryCursor);
}

AvocadoQueryCursor.prototype.toString = function () {  
  result  = "[object AvocadoQueryCursor]";
  return result;
}


////////////////////////////////////////////////////////////////////////////////
//
// AvocadoQueryInstance
//
////////////////////////////////////////////////////////////////////////////////

function AvocadoQueryInstance (database, data) {
  this._database = database;
  this.doCount = false;
  this.maxResults = null;
  this.bindVars = {};
  
  if (typeof data === "string") {
    this.query = data;
  }
  else if (data instanceof AvocadoQueryTemplate) {
    if (data.document.query == undefined) {
      data.load();
      if (data.document.query == undefined) {
        throw "could not load query string";
      }
    }
    this.query = data.document.query;
  }   
}

AvocadoQueryInstance.prototype.bind = function (key, value) {
  this.bindVars[key] = value;
}

AvocadoQueryInstance.prototype.setCount = function (bool) {
  if (bool) {
    this.doCount = true;
  }
  else {
    this.doCount = false;    
  }
}

AvocadoQueryInstance.prototype.setMax = function (value) {
  if (parseInt(value) > 0) {
    this.maxResults = parseInt(value);
  }
}

AvocadoQueryInstance.prototype.execute = function () {
  var body = {
    query : this.query,
    count : this.doCount,
    bindVars : this.bindVars
  }

  if (this.maxResults) {
    body["maxResults"] = this.maxResults;
  }
  
  var str = this._database._connection.post("/_api/cursor", JSON.stringify(body));

  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
    
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  return new AvocadoQueryCursor(this._database, requestResult);
}

AvocadoQueryInstance.prototype._help = function () {
  print(helpAvocadoQueryInstance);
}

AvocadoQueryInstance.prototype.toString = function () {  
  result  = "[object AvocadoQueryInstance]";
  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// AvocadoQueryTemplate
//
////////////////////////////////////////////////////////////////////////////////

function AvocadoQueryTemplate (database, data) {
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
    throw "AvocadoQueryTemplate needs query or _id.";
  }
}

AvocadoQueryTemplate.prototype.getInstance = function () {
  if (this.document._id == undefined) {
    throw "no _id found";    
  }
  
  return new AvocadoQueryInstance(this._database, this);
}

AvocadoQueryTemplate.prototype.update = function (data) {
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

AvocadoQueryTemplate.prototype.delete = function () {
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

AvocadoQueryTemplate.prototype.load = function () {
  if (this.document._id == undefined) {
    throw "AvocadoQueryTemplate needs _id for loading";    
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

AvocadoQueryTemplate.prototype.save = function () {
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

AvocadoQueryTemplate.prototype._help = function () {
  print(helpAvocadoQueryTemplate);
}

AvocadoQueryTemplate.prototype.toString = function () {  
  result  = "[object AvocadoQueryTemplate]";
  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// AvocadoDatabase
//
////////////////////////////////////////////////////////////////////////////////

function AvocadoDatabase (connection) {
  this._connection = connection;
}

AvocadoDatabase.prototype._collections = function () {
  var str = this._connection.get("/_api/collections");
  
  var requestResult = undefined;
  if (str != undefined) {
    requestResult = JSON.parse(str);
  }
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

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
  
  if (isErrorResult(requestResult)) {
    return undefined;
  }

  if (requestResult["name"] != undefined) {
    
    this[requestResult["name"]] = new AvocadoCollection(this, requestResult);
      
    return requestResult;
  }
  
  return undefined;
}

AvocadoDatabase.prototype.createQueryTemplate = function (queryData) {  
  var qt = new AvocadoQueryTemplate(this, queryData);
  if (qt.save()) {
    return qt;
  }
  
  return undefined;
}

AvocadoDatabase.prototype.getQueryTemplate = function (id) {  
  var qt = new AvocadoQueryTemplate(this, {"_id" : id});
  if (qt.load()) {
    return qt;
  }
  
  return undefined;
}

AvocadoDatabase.prototype.createQueryInstance = function (queryData) {  
  return new AvocadoQueryInstance(this, queryData);
}

AvocadoDatabase.prototype._help = function () {  
  print(helpAvocadoDatabase);
}

AvocadoDatabase.prototype.toString = function () {  
  result  = "[object AvocadoDatabase]";
  return result;
}

function printPlain(data) {
  var p = PRETTY_PRINT;
  PRETTY_PRINT = false;
  var c;
  if (typeof(COLOR_OUTPUT) != undefined) {
    c = COLOR_OUTPUT;
    COLOR_OUTPUT = undefined;
  }
  
  try {
   print(data);
   PRETTY_PRINT = p;
    if (typeof(c) != undefined) {
      COLOR_OUTPUT = c;
    }   
  }
  catch (e) {
   PRETTY_PRINT = p;
    if (typeof(c) != undefined) {
      COLOR_OUTPUT = c;
    }   
    throw e.message;    
  }  
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

  print(HELP);
}
catch (err) {
  print(COLOR_RED + "connection failure: " + err + COLOR_BLACK);
}
