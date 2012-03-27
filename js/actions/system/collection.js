var actions = require("actions");

function getCollection(req, res) {
  if (req.suffix.length != 1) {
    actions.actionResultError (req, res, 404, actions.collectionNotFound, "Collection not found");
    return;
  }
  
  var collection = decodeURIComponent(req.suffix[0]);

  try {
    var coll = db[collection];
    
    var result = {
      "_id" : coll._id,
      name : coll._name,
      status : coll.status(),
      figures : coll.figures()
    };
    
    actions.actionResultOK(req, res, 200, result);    
  }
  catch (e) {
    actions.actionResultError(req, res, 404, actions.collectionNotFound, "Collection not found") 
  }
}

actions.defineHttp({
  url : "_api/collection_OLD",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case ("GET") : 
        getCollection(req, res); 
        break;

      default:
        actions.actionResultUnsupported(req, res);
    }
  }
});
