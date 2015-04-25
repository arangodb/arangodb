// This is a generic CSV exporter for collections.
//
// Usage: Run with arangosh like this:
//   arangosh --javascript.execute <CollName> [ <Field1> <Field2> ... ]
//
// This will take all documents in the collection with name <CollName>
// and export a CSV file with a row for each document. Fields are
// specified by either simply giving their name, or in the form
//   <name>:<somestring>
// in which case the column header is the whole string but only the
// attribute with name <name> will be taken. This is in particular
// intended for data for neo4j, where there are type attributes after
// the colon. If the first letter of <name> is a "!", then it does
// not refer to an attribute name and the constant value of the string
// after the "!" is put into the corresponding column.
// If no fields are specified, then the tool guesses fields by
// inspecting the collection and a random document. It does know
// about edge collections in that case and does "something sensible"
// for export to neo4j in that case.

function format(x) {
  if (typeof(x) === "string") {
    return '"'+x+'"';
  }
  else if (typeof(x) === "object") {
    return '"'+JSON.stringify(x)+'"';
  }
  else if (typeof(x) === "undefined") {
    return "undefined";
  } 
  else {
    return x.toString();
  }
}

function type(x) {
  if (typeof(x) === "string") {
    return "string";
  }
  else if (typeof(x) === "number") {
    return "double";
  }
  else if (typeof(x) === "boolean") {
    return "boolean";
  } 
  else {
    return "string";
  }
}

function CSVExport (collName, fields, names) {
  var l = "";
  for (i = 0; i < fields.length; i++) {
    if (i > 0) {
      l += ",";
    }
    l += names[i];
  }
  print(l);

  var q = db._query('FOR d IN '+collName+' RETURN d');
  var n;
  while (q.hasNext()) {
    n = q.next();
    if (fields[0][0] === "!") {
      l = format(fields[0].substr(1));
    }
    else {
      l = format(n[fields[0]]);
    }
    for (i = 1; i < fields.length; i++) {
      if (fields[i][0] === "!") {
        l += "," + fields[i].substr(1);
      }
      else {
        l += "," + format(n[fields[i]]);
      }
    }
    print(l);
  }
}

var collName = ARGUMENTS[0];
var fields = [];
var names = [];

var i;

if (ARGUMENTS.length > 1) {
  for (i = 1; i < ARGUMENTS.length; i++) {
    var f = ARGUMENTS[i];
    var pos = f.indexOf(":");
    if (pos < 0) {
      fields.push(f);
      names.push(f);
    }
    else {
      names.push(f);
      fields.push(f.substr(0, pos));
    }
  }
} 
else {    // try to be clever:
  var c = db._collection(collName);
  var d = c.any();
  var itsFields = Object.keys(d);
  itsFields = itsFields.filter(function(s) { return s[0] !== "_"; });
  if (c.type() === 2) { // document-collection
    fields.push("_id");
    names.push("_id:ID");
  }
  else { // edge-collection
    fields.push("_from");
    names.push("_from:START_ID");
    fields.push("_to");
    names.push("_to:END_ID");
    fields.push("!EDGE");
    names.push(":TYPE");
  }
  for (i = 0; i < itsFields.length; i++) {
    fields.push(itsFields[i]);
    names.push(itsFields[i]+":"+type(d[itsFields[i]]));
  }
}
CSVExport(collName, fields, names);

