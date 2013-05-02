(function() {
  "use strict";
  var FoxxApplication = require("org/arangodb/foxx").Application,
    app = new FoxxApplication();
    
  app.registerRepository(
    "structs",
    {
      repository: "repositories/structures"
    }
  );
  
  app.get('/list', function (req, res) {
    res.json(repositories.structs.getMonitored());
  })
  .nickname("list")
  .summary("List all.")
  .notes("Gets a List of all monitored collections.");
  
  app.get('/structs/:name', function (req, res) {
    var name = req.params("name");
    res.json(repositories.structs.getStructs(name));
  })
  .nickname("structs")
  .pathParam("name", {
    description: "The name of a monitored collection",
    dataType: "string",
    required: true,
    multiple: false
  })
  .summary("Get Structs of collection")
  .notes("Gets the structural information of the given collection.");
  
  app.get('/content/:name', function (req, res) {
    var name = req.params("name");
    res.json(repositories.structs.getContent(name));
  })
  .nickname("content")
  .summary("Get the content")
  .notes("Collects the complete content of one collection.");
  
  //TODO!
  app.put("/content/:name/:key", function (req, res) {
    res.json({});
  });
  
  //TODO!
  app.del("/content/:name/:key", function (req, res) {
    res.json();
  });
  
  
  //TODO!
  app.post("/content/:name/", function (req, res) {
    var content = JSON.parse(req.requestBody);
    var key = Math.floor(Math.random() * 100000);
    content._key = key;
    content._id = req.params("name") + "/" + key;
    content._rev = key;
    res.json(content);
  });
  
  
  app.get('/test/list', function (req, res) {
    res.json([{
      name: "String"
    },{
      name: "Number"
    }])
  })
  .nickname("test")
  .summary("Test List")
  .notes("Test for List");
  
  
  app.get('/test/structs/:name', function (req, res) {
    var name = req.params("name");
    switch(name) {
      case "String":
        res.json({
          attributes: {
            name : {
              type: "string"
            }
          }
        });
        break;
      case "Number":
        res.json({
          attributes: {
            int : {
              type: "number"
            },
            double: {
              type: "number"
            }
          }
        });
        break;
      case "All":
        res.json({
          attributes: {
            int : {
              type: "number"
            },
            double: {
              type: "number"
            },
            name : {
              type: "string"
            }
          }
        });
        break;
      default:
        res.json("Error has to be done!");
    }
  })
  .nickname("test")
  .summary("Test Structs")
  .notes("Test for Structs");
  
  app.get('/test/content/:name', function (req, res) {
    var name = req.params("name");
    switch(name) {
      case "String":
        res.json([{
          _id: "String/1",
          _rev: "1",
          _key: "1",
          name: "Test"
        }]);
        break;
      case "Number":
        res.json([{
          _id: "Number/1",
          _rev: "1",
          _key: "1",
          int: 4,
          double: 4.5
        }]);
        break;
      case "All":
        res.json([{
          _id: "All/1",
          _rev: "1",
          _key: "1",
          int: 4,
          double: 4.5,
          name: "Test"
        }]);
        break;
      default:
        res.json("Error has to be done!");
    }
  })
  .nickname("test")
  .summary("Test Content")
  .notes("Test for Content");
  
  app.start(applicationContext);
}());