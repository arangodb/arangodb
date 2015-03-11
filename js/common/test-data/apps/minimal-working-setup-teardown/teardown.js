var db = require("internal").db;
var col = applicationContext.collectionName("setup_teardown");
db._drop(col);
