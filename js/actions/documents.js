function getDocuments(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found");
    return;
  }
  
  var collection = decodeURIComponent(req._suffix[0]);
  var skip = null;
  var limit = null;

    if (req.skip != undefined) {
      skip = parseInt(req.skip);
      if (skip < 0) {
        skip = 0;
      }
    }

    if (req.limit != undefined) {
      limit = parseInt(req.limit);
      if (limit < 0) {
        limit = 0;
      }
    }    

  try {  
    var result = db[collection].ALL(skip, limit);
    actionResultOK(req, res, 200, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found") 
  }
}

defineAction("_api/documents",
  function (req, res) {
    
    switch (req._requestType) {
      case ("GET") : 
        getDocuments(req, res); 
        break;
      default:
        actionResultUnsupported(req, res);
    }
    
  },
  {
    parameters : {
      skip : "number",
      limit : "number"
    }
  }
);
