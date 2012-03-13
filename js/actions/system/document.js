var actions = require("actions");

function getDocument(req, res) {
  if (req.suffix.length != 2) {
    actions.actionResultError (req, res, 404, actions.documentNotFound, "Document not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req.suffix[0]);
    var documentId = decodeURIComponent(req.suffix[1]);
    var result = {
      "document" : {}
    };
    
    result.document = db[collection].document(documentId);
    actions.actionResultOK(req, res, 200, result);    
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.documentNotFound, "Document not found: " + e);
  }
}

function deleteDocument(req, res) {
  if (req.suffix.length != 2) {
    actions.actionResultError (req, res, 404, actions.documentNotFound, "Document not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req.suffix[0]);
    var documentId = decodeURIComponent(req.suffix[1]);
    
    var result = {};
  
    
    if (db[collection].delete(documentId)) {
      result = { 
        "deleted" : true,
        "_id" : documentId
      };

      actions.actionResultOK(req, res, 200, result);          
    }
    else {
      actions.actionResultError (req, res, 304, actions.documentNotModified, "Document not deleted");      
    }
  }
  catch (e) {
    actions.actionResultError(req, res, 304, actions.documentNotModified, "Document not deleted: " + e);
  }
}

function postDocument(req, res) {
  if (req.suffix.length != 1) {
    actions.actionResultError (req, res, 404, actions.collectionNotFound, "Collection not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req.suffix[0]);
    var json = JSON.parse(req.requestBody);
    var id = db[collection].save(json);

    var result = {
      "created" : true,
      "_id" : id
    };
      
    actions.actionResultOK(req, res, 201, result);    
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.documentNotModified, "Document not saved: " + e);
  }
}

function putDocument(req, res) {
  if (req.suffix.length != 2) {
    actions.actionResultError (req, res, 404, actions.documentNotFound, "Document not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req.suffix[0]);
    var documentId = decodeURIComponent(req.suffix[1]);
    var json = JSON.parse(req.requestBody);
    var id = db[collection].replace(documentId, json);

    var result = {
      "updated" : true,
      "_id" : id
    };
      
    actions.actionResultOK(req, res, 202, result);    
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.documentNotModified, "Document not changed: " + e);
  }
}

actions.defineHttp({
  url : "_api/document",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case ("GET") : 
        getDocument(req, res); 
        break;

      case ("POST") : 
        postDocument(req, res); 
        break;

      case ("PUT") : 
        putDocument(req, res); 
        break;

      case ("DELETE") : 
        deleteDocument(req, res); 
        break;

      default:
        actions.actionResultUnsupported(req, res);
    }
  }
});
