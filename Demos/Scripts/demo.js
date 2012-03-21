#! avocsh

var console = require("console");
var name = "demo";

var collection = db[name];

collection.drop(name);

collection.save({ hallo : "world" });

collection.save({ world : "hallo" });

collection.save({ name : "Hugo", 
                  firstName : "Egon",
                  address : {
                    street : "Neue Strasse",
                    city : "Hier" },
                  hobbies : [ "swimming", "programming" ]});
