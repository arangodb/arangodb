arangoHelper = {
  systemAttributes: function () {
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
  },

  isSystemAttribute: function (val) {
    var a = this.systemAttributes();
    return a[val];
  },

  isSystemCollection: function (val) {
    return val && val.name && val.name.substr(0, 1) === '_';
  },

  collectionApiType: function (identifier) {
    if (CollectionTypes[identifier] == undefined) {
      CollectionTypes[identifier] = getCollectionInfo(identifier).type;
    }

    if (CollectionTypes[identifier] == 3) {
      return "edge";
    }
    return "document";
  },

  collectionType: function (val) {
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

};
