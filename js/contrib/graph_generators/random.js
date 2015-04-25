function makeRandomGraph (Vname, Ename, nV, nE) {
  var db = require("internal").db;
  db._drop(Vname);
  db._drop(Ename);
  var V = db._create(Vname);
  var E = db._createEdgeCollection(Ename);
  var x, y;
  var i;
  for (i = 0; i < nV; i++) {
    V.insert({_key: "X"+i});
  }
  for (i = 0;i < nE; i++) {
    x = Math.floor(Math.random() * nV);
    y = Math.floor(Math.random() * nV);
    E.insert(Vname+"/X"+x, Vname+"/X"+y, {_key: "Y"+i});
  }
}

