var actions = require("actions");

function getDocuments(req, res) {
  if (req.suffix.length != 1) {
    actions.resultError (req, res, 404, actions.collectionNotFound, "Collection not found");
    return;
  }
  
  var collection = decodeURIComponent(req.suffix[0]);
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
    var result = internal.db[collection].ALL(skip, limit);
    actions.resultOk(req, res, 200, result);    
  }
  catch (e) {
    actions.resultError (req, res, 404, actions.collectionNotFound, "Collection not found") 
  }
}

actions.defineHttp({
  url : "_api/documents",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case ("GET") : 
        getDocuments(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  },

  parameters : {
    skip : "number",
    limit : "number"
  }
});
