#! avocsh

var console = require("console");
var name = "geo";

var collection = db[name];

collection.drop(name);

db.geo.ensureGeoIndex("home");
db.geo.ensureGeoIndex("work.l", "work.b");

for (i = -90;  i <= 90;  i += 10) {
  for (j = -180; j <= 180; j += 10) {
    db.geo.save({
      name : "Name/" + i + "/" + j,
      home : [ i, j ],
      work : { b : i, l : j }
    });
  }
}

