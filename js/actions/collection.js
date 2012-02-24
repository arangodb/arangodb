function getCollections(req, res) {
    var colls;
    var coll;
    var result;

    colls = db._collections();
    result = {
      path : db._path,
      collections : {}
    };

    for (var i = 0;  i < colls.length;  ++i) {
      coll = colls[i];

      result.collections[coll._name] = {
        id : coll._id,
        name : coll._name,
        status : coll.status(),
        figures : coll.figures()
      };
    }

    actionResultOK(req, res, 200, result);  
}

function getCollection(req, res) {
  if (req._suffix.length != 1) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found");
    return;
  }

  try {
    var name = decodeURIComponent(req._suffix[0]);
    var result = {
      "name" : name,
      "documents" : {}
    };
  
    result.documents = db[name].all().toArray();
    actionResultOK(req, res, 200, result);    
  }
  catch (e) {
    actionResultError (req, res, 404, collectionNotFound, "Collection not found") 
  }
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
      blocksize : "number",
      page : "number"
    }
  }
);
