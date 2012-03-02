function getDocument(req, res) {
  if (req._suffix.length != 2) {
    actionResultError (req, res, 404, documentNotFound, "Document not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req._suffix[0]);
    var documentId = decodeURIComponent(req._suffix[1]);
    var result = {
      "document" : {}
    };
  
    
    result.document = db[collection].document(documentId);
    actionResultOK(req, res, 200, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, documentNotFound, "Document not found: " + e);
  }
}

function deleteDocument(req, res) {
  if (req._suffix.length != 2) {
    actionResultError (req, res, 404, documentNotFound, "Document not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req._suffix[0]);
    var documentId = decodeURIComponent(req._suffix[1]);
    
    var result = {};
  
    
    if (db[collection].delete(documentId)) {
      result = { 
        "deleted" : true,
        "_id" : documentId
      };
      actionResultOK(req, res, 200, result);          
    }
    else {
      actionResultError (req, res, 304, documentNotModified, "Document not deleted");      
    }
  }
  catch (e) {
    actionResultError(req, res, 304, documentNotModified, "Document not deleted: " + e);
  }
}

function postDocument(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req._suffix[0]);
    var json = JSON.parse(req._requestBody);
    var id = db[collection].save(json);

    var result = {
      "created" : true,
      "_id" : id
    };
      
    actionResultOK(req, res, 201, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, documentNotModified, "Document not saved: " + e);
  }
}

function putDocument(req, res) {
  if (req._suffix.length != 2) {
    actionResultError (req, res, 404, documentNotFound, "Document not found");
    return;
  }

  try {
    var collection = decodeURIComponent(req._suffix[0]);
    var documentId = decodeURIComponent(req._suffix[1]);
    var json = JSON.parse(req._requestBody);
    var id = db[collection].replace(documentId, json);

    var result = {
      "updated" : true,
      "_id" : id
    };
      
    actionResultOK(req, res, 202, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, documentNotModified, "Document not changed: " + e);
  }
}

defineAction("_api/document",
  function (req, res) {
    
    switch (req._requestType) {
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
        actionResultUnsupported(req, res);
    }
    
  },
  {
    parameters : {
    }
  }
);
