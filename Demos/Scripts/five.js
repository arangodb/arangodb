#! avocsh

var console = require("console");
var name = "five";

var collection = db[name];

collection.drop(name);
collection.ensureUniqueConstraint("a");
collection.ensureUniqueConstraint("b");

collection.save({ a : 1, b : "One" });
collection.save({ a : 2, b : "Two" });
collection.save({ a : 3, b : "Three" });
collection.save({ a : 4, b : "Four" });
collection.save({ a : 5, b : "Five" });
