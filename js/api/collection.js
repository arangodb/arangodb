function getCollection(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found");
    return;
  }
  
  var collection = decodeURIComponent(req._suffix[0]);

  try {
    var coll = db[collection];
    
    var result = {
      "_id" : coll._id,
      name : coll._name,
      status : coll.status(),
      figures : coll.figures()
    };
    
    actionResultOK(req, res, 200, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found") 
  }
}

defineAction("_api/collection",
  function (req, res) {
    
    switch (req._requestType) {
      case ("GET") : 
        getCollection(req, res); 
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
