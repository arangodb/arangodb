defineAction("examples",
  function (req, res) {
    var result = req.collection.all();

    queryResult(req, res, result);
  },
  {
    parameters : {
      collection : "collection",
      blocksize : "number",
      page : "number"
    }
  }
);

defineAction("examples/id",
  function (req, res) {
    var result = req.collection.all();

    queryReferences(req, res, result);
  },
  {
    parameters : {
      collection : "collection",
      blocksize : "number",
      page : "number"
    }
  }
);
