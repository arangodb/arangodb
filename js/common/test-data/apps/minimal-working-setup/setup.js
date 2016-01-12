var db = require("internal").db;
var col = applicationContext.collectionName("setup");
if (!db._collection(col)) {
  db._create(col);
}
