defineAction("system/collections.json",
  function (req, res) {
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

      result.collections[coll._name] = { name : coll._name, status : coll.status() };
    }

    res.responseCode = 200;
    res.contentType = "application/json";
    res.body = toJson(result);
  }
);
