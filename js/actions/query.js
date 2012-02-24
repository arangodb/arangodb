function getQuery(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, queryNotFound, "Query not found") 
    return;
  }

  try {
    var qid = decodeURIComponent(req._suffix[0]);           
    var result = db.query.document_wrapped(qid);
    
    var result = { 
      "query" : doc.query 
    };
    
    actionResultOK(req, res, 200, result);      
  }
  catch (e) {
    actionResultError (req, res, 404, queryNotFound, "Query not found") 
  }
}

function postQuery(req, res) {
  if (req._suffix.length != 0) {
    actionResultError(req, res, 404, queryNotModified, "Query not created") 
    return;
  }

  try {
    var json = JSON.parse(req._requestBody);
    
    if (json.query == undefined) {
      actionResultError(req, res, 404, queryNotModified, "Document has no query.") 
      return;      
    }
    
    var q = {
      "query":json.query
    }
    
    var id = db.query.save(q);
    
    var result = { 
      "qid" : id 
    };
    
    actionResultOK(req, res, 201, result);        
  }
  catch (e) {
    actionResultError (req, res, 404, queryNotModified, "Query not created") 
  }
}

function putQuery(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, queryNotFound, "Query not found");
    return;
  }

  try {
    var qid = decodeURIComponent(req._suffix[0]);
    var json = JSON.parse(req._requestBody);

    if (json.query == undefined) {
      actionResultError(req, res, 404, queryNotModified, "Document has no query.") 
      return;      
    }
    
    var q = {
      "query":json.query
    }

    var id = db.query.replace(qid, q);

    var result = {
      "qid" : id
    };
      
    actionResultOK(req, res, 202, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, queryNotModified, "Query not changed") 
  }
}

function deleteQuery(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, queryNotFound, "Query not found");
    return;
  }

  try {
    var qid = decodeURIComponent(req._suffix[0]);
    
    if (db.query.delete(qid)) {
      actionResultOK(req, res, 202, {"qid" : qid});          
    }
    else {
      actionResultError (req, res, 404, queryNotModified, "Query not changed") 
    }
      
  }
  catch (e) {
    actionResultError (req, res, 404, queryNotModified, "Query not changed") 
  }
}

defineAction("_api/query",
  function (req, res) {
    
    switch (req._requestType) {
      case ("GET") : 
        getQuery(req, res); 
        break;
      case ("POST") : 
        postQuery(req, res); 
        break;
      case ("PUT") :  
        putQuery(req, res); 
        break;
      case ("DELETE") :  
        deleteQuery(req, res); 
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
