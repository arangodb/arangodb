
function postCursor(req, res) {
  if (req._suffix.length != 0) {
    actionResultError (req, res, 404, cursorNotModified, "Cursor not created") 
    return;
  }

  try {
    var json = JSON.parse(req._requestBody);

    if (json.qid == undefined) {
      actionResultError (req, res, 404, cursorNotModified, "Missing query identifier");
      return;
    }

    var cid = db.cursor.save(json);

    // req.bindParameter
    // req.skip
    // req.count    
    // query.execute(...)
    
    var result = { 
      "result" : [{"found":"nothing"}],
      "cid" : cid,
      "count" : 1,
      "hasMore" : false
    };
    
    actionResultOK(req, res, 201, result);        
  }
  catch (e) {
    actionResultError (req, res, 404, cursorNotModified, "Cursor not created") 
  }
}

function putCursor(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found") 
    return;
  }

  try {
    var cid = decodeURIComponent(req._suffix[0]);           
    var data = db.cursor.document_wrapped(qid);
    
    // TODO

    var result = { 
      "result" : [{"found":"nothing"}],
      "count" : 1,
      "cid" : cid,
      "hasMore" : false
    };
    
    actionResultOK(req, res, 200, result);
  }
  catch (e) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found") 
  }
}

function deleteCursor(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found") 
    return;
  }

  try {
    var cid = decodeURIComponent(req._suffix[0]);
    if(db.cursor.delete(qid)) {
      actionResultOK(req, res, 202, {"cid" : cid});                
    }
    else {
      actionResultError (req, res, 404, cursorNotFound, "Cursor not found")       
    }
  }
  catch (e) {
    actionResultError (req, res, 404, cursorNotFound, "Cursor not found") 
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
