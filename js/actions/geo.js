defineAction("near",
  function (req, res) {
               var result = req.collection.near(req.lat, req.lon).distance();

    queryResult(req, res, result);
   },
   {
     parameters : {
       collection : "collection",
       lat : "number",
       lon : "number"
     }
   }
);

defineAction("within",
  function (req, res) {
               var result = req.collection.within(req.lat, req.lon, req.radius).distance();

    queryResult(req, res, result);
   },
   {
     parameters : {
       collection : "collection",
       lat : "number",
       lon : "number",
       radius : "number"
     }
   }
);

