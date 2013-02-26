arangoHelper = {
  CollectionTypes: {},
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
  getRandomToken: function () {
    return Math.round(new Date().getTime());
  },

  isSystemAttribute: function (val) {
    var a = this.systemAttributes();
    return a[val];
  },

  isSystemCollection: function (val) {
    //return val && val.name && val.name.substr(0, 1) === '_';
    return val.substr(0, 1) === '_';
  },

  collectionApiType: function (identifier) {
    if (this.CollectionTypes[identifier] == undefined) {
      this.CollectionTypes[identifier] = window.arangoDocumentStore.getCollectionInfo(identifier).type;
    }

    if (this.CollectionTypes[identifier] == 3) {
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

    if (val.name.substr(0, 1) === '_') {
      type += " (system)";
    }

    return type;
  }

};
