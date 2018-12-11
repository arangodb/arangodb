
db._drop("demo");
db._create("demo");
collectionAlreadyThere.push("demo");
db.demo.save({
  "_key" : "schlonz",
  "firstName" : "Hugo",
  "lastName" : "Schlonz",
  "address" : {
   "street" : "Strasse 1",
    "city" : "Hier"
  },
  "hobbies" : [
    "swimming",
    "biking",
    "programming"
  ]
});

db._drop("animals");
db._create("animals"); 
collectionAlreadyThere.push("animals");

db._dropView("demoView");
db._createView("demoView", "arangosearch");
collectionAlreadyThere.push("demoView");
