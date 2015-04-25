function makeName (x, y) {
  return "X"+x+"Y"+y;
}

function makeGrid (Vname, Ename, n) {
  var db = require("internal").db;
  db._drop(Vname);
  db._drop(Ename);
  var V = db._create(Vname);
  var E = db._createEdgeCollection(Ename);
  var x, y;
  for (x = 0; x < n; x++) {
    for (y = 0; y < n; y++) {
      V.insert({x:x, y:y, _key: makeName(x, y)});
    }
  }
  // The horizontal edges:
  for (y = 0; y < n; y++) {
    for (x = 0; x < n-1; x++) {
      E.insert(Vname+"/"+makeName(x,y), Vname+"/"+makeName(x+1,y), 
               {_key: "H"+makeName(x,y)});
    }
  }
  // The vertical edges:
  for (x = 0; x < n; x++) {
    for (y = 0; y < n-1; y++) {
      E.insert(Vname+"/"+makeName(x,y), Vname+"/"+makeName(x,y+1), 
               {_key: "V"+makeName(x,y)});
    }
  }
}

