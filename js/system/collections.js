var actions = require("actions");

function getCollections(req, res) {
  var colls;
  var coll;
  var result;


  colls = db._collections();

  var skip = 0;
  var end = colls.length;

  if (req.parameters != undefined) {
    if (req.parameters["skip"] != undefined) {
      skip = parseInt(req.parameters["skip"]);
      if (skip < 0) {
        skip = 0;
      }
    }

    if (req.parameters["limit"] != undefined) {      
      var limit = parseInt(req.parameters["limit"]);
      if (limit < 0) {
        end = colls.length;
      }
      else {
        end = Math.min(colls.length, skip + limit)
      }
    }    
  }

  var count = 0;
  if (skip < end) {
    count = end - skip;
  }

  result = {
    path : db._path,
    total : colls.length,
    count : count,
    collections : {}
  };

  for (var i = skip;  i < end;  ++i) {
    coll = colls[i];
    result.collections[coll._name] = {
      _id : coll._id,
      name : coll._name,
      status : coll.status(),
      figures : coll.figures()
    };
  }

  actions.actionResultOK(req, res, 200, result);  
}

actions.defineHttp({
  url : "_api/collections",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case ("GET") : 
        getCollections(req, res); 
        break;

      default:
        actions.actionResultError (req, res, 405, 405, "Unsupported method");
    }
  }
});
