

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
