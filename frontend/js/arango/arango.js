/*global functions */

function systemAttributes () {
  return {
    '_id' : true,
    '_rev' : true,
    '_key' : true,
    '_from' : true,
    '_to' : true,
    '_bidirectional' : true,
    '_vertices' : true,
    '_from' : true,
    '_to' : true,
    '$id' : true
  };
}

function isSystemAttribute (val) {
  var a = systemAttributes();
  return a[val];
}

function isSystemCollection (val) {
  return val && val.name && val.name.substr(0, 1) === '_';
}

function collectionApiType (identifier) {
  if (CollectionTypes[identifier] == undefined) {
    CollectionTypes[identifier] = getCollectionInfo(identifier).type;
  }

  if (CollectionTypes[identifier] == 3) {
    return "edge";
  }
  return "document";
}

function collectionType (val) {
  if (! val || val.name == '') {
    return "-";
  }

  var type;
  if (val.type == 2) {
    type = "document";
  }
  else if (val.type == 3) {
    type = "edge";
  }
  else {
    type = "unknown";
  }

  if (isSystemCollection(val)) {
    type += " (system)";
  }

  return type;
}


