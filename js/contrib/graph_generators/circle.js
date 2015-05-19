function makeCircle(Vname, Ename, prefix, N) {
  var db = require("internal").db;
  db._drop(Vname);
  db._drop(Ename);
  var V = db._create(Vname);
  var E = db._createEdgeCollection(Ename);
  var i;
  for (i = 1; i <= N; i++) {
    V.insert({_key: prefix + i, nr: i});
  }
  for (i = 1; i < N; i++) {
    E.insert(Vname+"/"+prefix+i, Vname+"/"+prefix+(i+1), {_key:prefix+i});
  }
  E.insert(Vname+"/"+prefix+N, Vname+"/"+prefix+1, {_key: prefix+N});
}
