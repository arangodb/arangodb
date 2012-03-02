function getCollections(req, res) {
  var colls;
  var coll;
  var result;


  colls = db._collections();

  var skip = 0;
  var end = colls.length;

  if (req._parameters != undefined) {
    if (req._parameters["skip"] != undefined) {
      skip = parseInt(req._parameters["skip"]);
      if (skip < 0) {
        skip = 0;
      }
    }

    if (req._parameters["limit"] != undefined) {      
      var limit = parseInt(req._parameters["limit"]);
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

  actionResultOK(req, res, 200, result);  
}

defineAction("_api/collections",
  function (req, res) {
    
    switch (req._requestType) {
      case ("GET") : 
        getCollections(req, res); 
        break;
      default:
        actionResultError (req, res, 405, 405, "Unsupported method");
    }
    
  },
  {
    parameters : {
    }
  }
);
