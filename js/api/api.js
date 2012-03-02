////////////////////////////////////////////////////////////////////////////////
/// @brief returns an error
///
/// @FUN{actionResultError(@FA{req}, @FA{res}, @FA{httpReturnCode}, @FA{errorNum}, @FA{errorMessage}, @FA{headers})}
///
/// The functions returns an error json object. The returned object is an array 
/// with an attribute @LIT{errorMessage} containing the error message 
/// @FA{errorMessage}.
////////////////////////////////////////////////////////////////////////////////

function actionResultError (req, res, httpReturnCode, errorNum, errorMessage, headers) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json";
  
  var result = {
    "error"        : true,
    "code"         : httpReturnCode,
    "errorNum"     : errorNum,
    "errorMessage" : errorMessage
  }
  
  res.body = JSON.stringify(result);

  if (headers != undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an error for unsupported methods
///
/// @FUN{actionResultUnsupported(@FA{req}, @FA{res}, @FA{headers})}
///
/// The functions returns an error json object. The returned object is an array 
/// with an attribute @LIT{errorMessage} containing the error message 
/// @FA{errorMessage}.
////////////////////////////////////////////////////////////////////////////////

function actionResultUnsupported (req, res, headers) {
  actionResultError(req, res, 405, 405, "Unsupported method", headers);  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result
///
/// @FUN{actionResultOK(@FA{req}, @FA{res}, @FA{httpReturnCode}, @FA{result}, @FA{headers}})}
///
////////////////////////////////////////////////////////////////////////////////

function actionResultOK (req, res, httpReturnCode, result, headers) {  
  res.responseCode = httpReturnCode;
  res.contentType = "application/json";
  
  // add some default attributes to result:
  result.error = false;  
  result.code = httpReturnCode;
  
  res.body = JSON.stringify(result);
  
  if (headers != undefined) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief error codes 
////////////////////////////////////////////////////////////////////////////////

var queryNotFound = 10404;
var queryNotModified = 10304;

var collectionNotFound = 20404;
var documentNotFound = 30404;
var documentNotModified = 30304;

var cursorNotFound = 40404;
var cursorNotModified = 40304;

var myApiRequests = {};

myApiRequests.cursor = {
        "POST /_api/cursor" : "create and execute query. (creates a cursor)",
        "PUT /_api/cursor/<cursor-id>" : "get next results",
        "DELETE /_api/cursor/<cursor-id>" : "delete cursor"  
}      

myApiRequests.collection = {
        "GET /_api/collections" : "get list of collections",
        "GET /_api/collection/<collection-id>" : "get all elements of collection"  
}      

myApiRequests.document = {
        "POST /_api/document/<collection-id>" : "create new document",
        "PUT /_api/document/<collection-id>/<document-id>" : "update document",
        "GET /_api/document/<collection-id>/<document-id>" : "get a document",        
        "DELETE /_api/document/<collection-id>/<document-id>" : "delete a document"
}      

myApiRequests.query = {
        "POST /_api/query" : "create a query",
        "GET /_api/query/<query-id>" : "get query",
        "PUT /_api/query/<query-id>" : "change query",
        "DELETE /_api/query/<query-id>" : "delete query"  
}      

defineAction("help",
  function (req, res) {
    var result = {
      "requests":myApiRequests
    }
    
    actionResultOK(req, res, 200, result);    
  },
  {
    parameters : {
    }
  }
);
