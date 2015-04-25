function makeTree(Vname, Ename, prefix, N, rev) {
  var db = require("internal").db;
  var V = db._collection(Vname);
  var E = db._collection(Ename);
  var i;
  for (i = 1; i <= Math.pow(2,N)-1; i++) {
    V.insert({_key: prefix + i, nr: i});
  }
  for (i = 1; i <= Math.pow(2,N-1)-1; i++) {
    [2*i, 2*i+1].forEach(function(j) {
      if (j < Math.pow(2,N)) {
        if (! rev) {
          E.insert(Vname+"/"+prefix+i, Vname+"/"+prefix+j,
                   {_key: prefix + j });
        }
        else {
          E.insert(Vname+"/"+prefix+j, Vname+"/"+prefix+i,
                   {_key: prefix + j });
        }
      }
    });
  }
}

function makeTreeGraph (Vname, Ename, prefix, n) {
  var db = require("internal").db;
  db._drop(Vname);
  db._drop(Ename);
  var V = db._create(Vname);
  var E = db._createEdgeCollection(Ename);
  makeTree(Vname, Ename, prefix+"_out", 2*n, false);
  makeTree(Vname, Ename, prefix+"_in", 2*n, true);
  E.insert(Vname+"/"+prefix+"_out"+(Math.pow(2,n)-1),
           Vname+"/"+prefix+"_in"+(Math.pow(2,n)-1),
           {_key:"special"});
}
