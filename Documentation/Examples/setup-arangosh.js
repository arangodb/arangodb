db._drop("demo");
db._create("demo");
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
