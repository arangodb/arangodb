
function postCursor(req, res) {
  if (req._suffix.length != 0) {
    actionResultError (req, res, 404, cursorNotModified, "Cursor not created"); 
    return;
  }

  try {
    var json = JSON.parse(req._requestBody);
    var queryString;

    if (json.qid != undefined) {
      var q = db.query.document_wrapped(json.qid);
      queryString = q.query;
    }    
    else if (json.query != undefined) {
      queryString = json.query;
    }

    if (queryString == undefined) {
      actionResultError (req, res, 404, cursorNotModified, "Missing query identifier");
      return;
    }
   
    var result = AQL_PREPARE(db, queryString);
    if (result instanceof AvocadoQueryError) {
      actionResultError (req, res, 404, result.code, result.message);
      return;
    }

    queryLimit = 0;
    var cursor = result.execute();
    
    var lines = [];    
    var i = 0;
    
    while (cursor.hasNext()) {
      lines[i] = cursor.next();
      ++i;
    }
    
    // req.bindParameter
    // req.skip
    // req.count    
    // query.execute(...)
    
    var hasMore = false;
    var cursorId = null;
    
    if (queryString == "select a from achim a") {
      hasMore = true;
      cursorId = "egal:egal";
    }
    
    var result = { 
      "result" : lines,
      "_id" : cursorId,
      "count" : i,
      "hasMore" : hasMore
    };
    
    actionResultOK(req, res, 201, result);        
  }
  catch (e) {
    actionResultError (req, res, 404, cursorNotModified, "Cursor not created");
  }
}

function putCursor(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found");
    return;
  }

  try {
    var cursorId = decodeURIComponent(req._suffix[0]);           
    
    // TODO

    var result = { 
      "result" : [{"test":"protest"}, {"hello":"world"}],
      "count" : 2,
      "_id" : cursorId,
      "hasMore" : false
    };
    
    actionResultOK(req, res, 200, result);
  }
  catch (e) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found");
  }
}

function deleteCursor(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found");
    return;
  }

  try {
    var cid = decodeURIComponent(req._suffix[0]);
    if(db.cursor.delete(qid)) {
      actionResultOK(req, res, 202, {"cid" : cid});                
    }
    else {
      actionResultError (req, res, 404, cursorNotFound, "Cursor not found");       
    }
  }
  catch (e) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found");
  }
}

defineAction("_api/cursor",
  function (req, res) {
    
    switch (req._requestType) {
      case ("POST") : 
        postCursor(req, res); 
        break;
      case ("PUT") :  
        putCursor(req, res); 
        break;
      case ("DELETE") :  
        deleteCursor(req, res); 
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
