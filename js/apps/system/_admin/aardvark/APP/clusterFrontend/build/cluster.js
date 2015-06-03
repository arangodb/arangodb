/*jshint unused: false */
/*global window, $, document */

(function() {
  "use strict";
  var isCoordinator;

  window.isCoordinator = function() {
    if (isCoordinator === undefined) {
      $.ajax(
        "cluster/amICoordinator",
        {
          async: false,
          success: function(d) {
            isCoordinator = d;
          }
        }
      );
    }
    return isCoordinator;
  };

  window.versionHelper = {
    fromString: function (s) {
      var parts = s.replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');
      return {
        major: parseInt(parts[0], 10) || 0,
        minor: parseInt(parts[1], 10) || 0,
        patch: parseInt(parts[2], 10) || 0,
        toString: function() {
          return this.major + "." + this.minor + "." + this.patch;
        }
      };
    },
    toString: function (v) {
      return v.major + '.' + v.minor + '.' + v.patch;
    }
  };

  window.arangoHelper = {
    lastNotificationMessage: null,

    CollectionTypes: {},
    systemAttributes: function () {
      return {
        '_id' : true,
        '_rev' : true,
        '_key' : true,
        '_bidirectional' : true,
        '_vertices' : true,
        '_from' : true,
        '_to' : true,
        '$id' : true
      };
    },

    fixTooltips: function (selector, placement) {
      $(selector).tooltip({
        placement: placement,
        hide: false,
        show: false
      });
    },

    currentDatabase: function () {
      var returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/database/current",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          returnVal = data.result.name;
        },
        error: function() {
          returnVal = false;
        }
      });
      return returnVal;
    },

    allHotkeys: {
      global: {
        name: "Site wide",
        content: [{
          label: "scroll up",
          letter: "j"
        },{
          label: "scroll down",
          letter: "k"
        }]
      },
      jsoneditor: {
        name: "AQL editor",
        content: [{
          label: "Submit",
          letter: "Ctrl + Return"
        },{
          label: "Toggle comments",
          letter: "Ctrl + Shift + C"
        },{
          label: "Undo",
          letter: "Ctrl + Z"
        },{
          label: "Redo",
          letter: "Ctrl + Shift + Z"
        }]
      },
      doceditor: {
        name: "Document editor",
        content: [{
          label: "Insert",
          letter: "Ctrl + Insert"
        },{
          label: "Save",
          letter: "Ctrl + Return, CMD + Return"
        },{
          label: "Append",
          letter: "Ctrl + Shift + Insert"
        },{
          label: "Duplicate",
          letter: "Ctrl + D"
        },{
          label: "Remove",
          letter: "Ctrl + Delete"
        }]
      },
      modals: {
        name: "Modal",
        content: [{
          label: "Submit",
          letter: "Return"
        },{
          label: "Close",
          letter: "Esc"
        },{
          label: "Navigate buttons",
          letter: "Arrow keys"
        },{
          label: "Navigate content",
          letter: "Tab"
        }]
      }
    },

    hotkeysFunctions: {
      scrollDown: function () {
        window.scrollBy(0,180);
      },
      scrollUp: function () {
        window.scrollBy(0,-180);
      },
      showHotkeysModal: function () {
        var buttons = [],
        content = window.arangoHelper.allHotkeys;

        window.modalView.show("modalHotkeys.ejs", "Keyboard Shortcuts", buttons, content);
      }
    },

    enableKeyboardHotkeys: function (enable) {
      var hotkeys = window.arangoHelper.hotkeysFunctions;
      if (enable === true) {
        $(document).on('keydown', null, 'j', hotkeys.scrollDown);
        $(document).on('keydown', null, 'k', hotkeys.scrollUp);
      }
    },

    databaseAllowed: function () {
      var currentDB = this.currentDatabase(),
      returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_db/"+ encodeURIComponent(currentDB) + "/_api/database/",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function() {
          returnVal = true;
        },
        error: function() {
          returnVal = false;
        }
      });
      return returnVal;
    },

    arangoNotification: function (title, content) {
      window.App.notificationList.add({title:title, content: content});
    },

    arangoError: function (title, content) {
      window.App.notificationList.add({title:title, content: content});
    },

    getRandomToken: function () {
      return Math.round(new Date().getTime());
    },

    isSystemAttribute: function (val) {
      var a = this.systemAttributes();
      return a[val];
    },

    isSystemCollection: function (val) {
      return val.name.substr(0, 1) === '_';
      // the below code is completely inappropriate as it will
      // load the collection just for the check whether it
      // is a system collection. as a consequence, the below
      // code would load ALL collections when the web interface
      // is called
      /*
         var returnVal = false;
         $.ajax({
type: "GET",
url: "/_api/collection/" + encodeURIComponent(val) + "/properties",
contentType: "application/json",
processData: false,
async: false,
success: function(data) {
returnVal = data.isSystem;
},
error: function(data) {
returnVal = false;
}
});
return returnVal;
*/
    },

    setDocumentStore : function (a) {
      this.arangoDocumentStore = a;
    },

    collectionApiType: function (identifier, refresh) {
      // set "refresh" to disable caching collection type
      if (refresh || this.CollectionTypes[identifier] === undefined) {
        this.CollectionTypes[identifier] = this.arangoDocumentStore
        .getCollectionInfo(identifier).type;
      }
      if (this.CollectionTypes[identifier] === 3) {
        return "edge";
      }
      return "document";
    },

    collectionType: function (val) {
      if (! val || val.name === '') {
        return "-";
      }
      var type;
      if (val.type === 2) {
        type = "document";
      }
      else if (val.type === 3) {
        type = "edge";
      }
      else {
        type = "unknown";
      }

      if (this.isSystemCollection(val)) {
        type += " (system)";
      }

      return type;
    },

    formatDT: function (dt) {
      var pad = function (n) {
        return n < 10 ? '0' + n : n;
      };

      return dt.getUTCFullYear() + '-'
      + pad(dt.getUTCMonth() + 1) + '-'
      + pad(dt.getUTCDate()) + ' '
      + pad(dt.getUTCHours()) + ':'
      + pad(dt.getUTCMinutes()) + ':'
      + pad(dt.getUTCSeconds());
    },

    escapeHtml: function (val) {
      // HTML-escape a string
      return String(val).replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
    }

  };
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCollection = Backbone.Model.extend({
    defaults: {
      "name": "",
      "status": "ok"
    },

    idAttribute: "name",

    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status")
      };
    }
  });
}());


/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCoordinator = Backbone.Model.extend({

    defaults: {
      "name": "",
      "url": "",
      "status": "ok"
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/Coordinators";

    updateUrl: function() {
      this.url = window.getNewRoute("Coordinators");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status"),
        url: this.get("url")
      };
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterDatabase = Backbone.Model.extend({

    defaults: {
      "name": "",
      "status": "ok"
    },

    idAttribute: "name",

    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status")
      };
    }
    /*
    url: "/_admin/aardvark/cluster/Databases";

    updateUrl: function() {
      this.url = window.getNewRoute("Databases");
    }
    */

  });
}());

/*global window, Backbone, $, _*/
(function() {
  "use strict";

  window.ClusterPlan = Backbone.Model.extend({

    defaults: {
    },

    url: "cluster/plan",

    idAttribute: "config",

    getVersion: function() {
      var v = this.get("version");
      return v || "2.0";
    },

    getCoordinator: function() {
      if (this._coord) {
        return this._coord[
          this._lastStableCoord
        ];
      }
      var tmpList = [];
      var i,j,r,l;
      r = this.get("runInfo");
      if (!r) {
        return;
      }
      j = r.length-1;
      while (j > 0) {
        if(r[j].isStartServers) {
          l = r[j];
          if (l.endpoints) {
            for (i = 0; i < l.endpoints.length;i++) {
              if (l.roles[i] === "Coordinator") {
                tmpList.push(l.endpoints[i]
                  .replace("tcp://","http://")
                  .replace("ssl://", "https://")
                );
              }
            }
          }
        }
        j--;
      }
      this._coord = tmpList;
      this._lastStableCoord = Math.floor(Math.random() * this._coord.length);
    },

    rotateCoordinator: function() {
      var last = this._lastStableCoord, next;
      if (this._coord.length > 1) {
        do {
          next = Math.floor(Math.random() * this._coord.length);
        } while (next === last);
        this._lastStableCoord = next;
      }
    },

    isAlive : function() {
      var result = false;
      $.ajax({
        cache: false,
        type: "GET",
        async: false, // sequential calls!
        url: "cluster/healthcheck",
        success: function(data) {
          result = data;
        },
        error: function(data) {
        }
      });
      return result;
    },

    storeCredentials: function(name, passwd) {
      var self = this;
      $.ajax({
        url: "cluster/plan/credentials",
        type: "PUT",
        data: JSON.stringify({
          user: name,
          passwd: passwd
        }),
        async: false
      }).done(function() {
        self.fetch();
      });
    },

    isSymmetricSetup: function() {
      var config = this.get("config");
      var count = _.size(config.dispatchers);
      return count === config.numberOfCoordinators
        && count === config.numberOfDBservers;
    },

    isTestSetup: function() {
      return _.size(this.get("config").dispatchers) === 1;
    },

    cleanUp: function() {
      $.ajax({
        url: "cluster/plan/cleanUp",
        type: "DELETE",
        async: false
      });
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterServer = Backbone.Model.extend({
    defaults: {
      name: "",
      address: "",
      role: "",
      status: "ok"
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/DBServers";

    updateUrl: function() {
      this.url = window.getNewRoute("DBServers");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        address: this.get("address"),
        status: this.get("status")
      };
    }

  });
}());


/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterShard = Backbone.Model.extend({
    defaults: {
    },

    idAttribute: "name",

    forList: function() {
      return {
        server: this.get("name"),
        shards: this.get("shards")
      };
    }
    /*
    url: "/_admin/aardvark/cluster/Shards";

    updateUrl: function() {
      this.url = window.getNewRoute("Shards");
    }
    */
  });
}());


/*global window, Backbone, $, _*/
(function() {

  "use strict";

  window.ClusterType = Backbone.Model.extend({

    defaults: {
      "type": "testPlan"
    }
  });
}());

/*global window, Backbone, console */
(function() {
  "use strict";

  window.AutomaticRetryCollection = Backbone.Collection.extend({

    _retryCount: 0,


    checkRetries: function() {
      var self = this;
      this.updateUrl();
      if (this._retryCount > 10) {
        window.setTimeout(function() {
          self._retryCount = 0;
        }, 10000);
        window.App.clusterUnreachable();
        return false;
      }
      return true;
    },

    successFullTry: function() {
      this._retryCount = 0;
    },

    failureTry: function(retry, ignore, err) {
      if (err.status === 401) {
        window.App.requestAuth();
      } else {
        window.App.clusterPlan.rotateCoordinator();
        this._retryCount++;
        retry();
      }
    }

  });
}());

/*global Backbone, window */
window.ClusterStatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,

  url: "/_admin/statistics",

  updateUrl: function() {
    this.url = window.App.getNewRoute("statistics");
  },

  initialize: function() {
    window.App.registerForUpdate(this);
  },

  // The callback has to be invokeable for each result individually
  fetch: function(callback, errCB) {
    this.forEach(function (m) {
      m.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: function() {
          errCB(m);
        }
      }).done(function() {
        callback(m);
      });
    });
  }
});

/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterCollections = window.AutomaticRetryCollection.extend({
    model: window.ClusterCollection,

    updateUrl: function() {
      this.url = window.App.getNewRoute(this.dbname + "/Collections");
    },

    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + "Collections";
    },

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
      window.App.registerForUpdate(this);
    },

    getList: function(db, callback) {
      if (db === undefined) {
        return;
      }
      this.dbname = db;
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, db, callback))
      }).done(function() {
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    },

    stopUpdating: function () {
      window.clearTimeout(this.timer);
      this.isUpdating = false;
    },

    startUpdating: function () {
      if (this.isUpdating) {
        return;
      }
      this.isUpdating = true;
      var self = this;
      this.timer = window.setInterval(function() {
        self.updateUrl();
        self.fetch({
          beforeSend: window.App.addAuth.bind(window.App)
        });
      }, this.interval);
    }

  });
}());




/*global window, Backbone, console */
(function() {
  "use strict";
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,

    url: "/_admin/aardvark/cluster/Coordinators",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Coordinators");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning":
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default:
          return "danger";
      }
    },

    getStatuses: function(cb, nextStep) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb, nextStep))
      }).done(function() {
        self.successFullTry();
        self.forEach(function(m) {
          cb(self.statusClass(m.get("status")), m.get("address"));
        });
        nextStep();
      });
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].coords = res[addr].coords || [];
          res[addr].coords.push(m);
        });
        callback(res);
      });
    },

    checkConnection: function(callback) {
      var self = this;
      if(!this.checkRetries()) {
        return;
      }
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.checkConnection.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        callback();
      });
    },

    getList: function() {
      throw "Do not use coordinator.getList";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      return this.map(function(m) {
        return m.forList();
      });
      */
    },

    getOverview: function() {
      throw "Do not use coordinator.getOverview";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      var res = {
        plan: 0,
        having: 0,
        status: "ok"
      },
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      this.each(function(m) {
        res.plan++;
        switch (m.get("status")) {
          case "ok":
            res.having++;
            break;
          case "warning":
            res.having++;
            updateStatus("warning");
            break;
          case "critical":
            updateStatus("critical");
            break;
          default:
            console.debug("Undefined server state occured. This is still in development");
        }
      });
      return res;
      */
    }
  });
}());



/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterDatabases = window.AutomaticRetryCollection.extend({

    model: window.ClusterDatabase,

    url: "/_admin/aardvark/cluster/Databases",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Databases");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    getList: function(callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    }

  });
}());

/*global window, Backbone, _, console */
(function() {

  "use strict";

  window.ClusterServers = window.AutomaticRetryCollection.extend({

    model: window.ClusterServer,

    url: "/_admin/aardvark/cluster/DBServers",

    updateUrl: function() {
      this.url = window.App.getNewRoute("DBServers");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning":
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default:
          return "danger";
      }
    },

    getStatuses: function(cb) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this,
        completed = function() {
          self.successFullTry();
          self._retryCount = 0;
          self.forEach(function(m) {
            cb(self.statusClass(m.get("status")), m.get("address"));
          });
        };
      // This is the first function called in
      // Each update loop
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb))
      }).done(completed);
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].dbs = res[addr].dbs || [];
          res[addr].dbs.push(m);
        });
        callback(res);
      });
    },

    getList: function(callback) {
      throw "Do not use";
      /*
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        var res = [];
        _.each(self.where({role: "primary"}), function(m) {
          var e = {};
          e.primary = m.forList();
          if (m.get("secondary")) {
            e.secondary = self.get(m.get("secondary")).forList();
          }
          res.push(e);
        });
        callback(res);
      });
      */
    },

    getOverview: function() {
      throw "Do not use DbServer.getOverview";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      var res = {
        plan: 0,
        having: 0,
        status: "ok"
      },
      self = this,
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      _.each(this.where({role: "primary"}), function(m) {
        res.plan++;
        switch (m.get("status")) {
          case "ok":
            res.having++;
            break;
          case "warning":
            res.having++;
            updateStatus("warning");
            break;
          case "critical":
            var bkp = self.get(m.get("secondary"));
            if (!bkp || bkp.get("status") === "critical") {
              updateStatus("critical");
            } else {
              if (bkp.get("status") === "ok") {
                res.having++;
                updateStatus("warning");
              }
            }
            break;
          default:
            console.debug("Undefined server state occured. This is still in development");
        }
      });
      return res;
      */
    }
  });

}());


/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterShards = window.AutomaticRetryCollection.extend({

    model: window.ClusterShard,

    updateUrl: function() {
      this.url = window.App.getNewRoute(
        this.dbname + "/" + this.colname + "/Shards"
      );
    },

    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + this.colname + "/"
        + "Shards";
    },

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
      window.App.registerForUpdate(this);
    },

    getList: function(dbname, colname, callback) {
      if (dbname === undefined || colname === undefined) {
        return;
      }
      this.dbname = dbname;
      this.colname = colname;
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(
          self, dbname, colname, callback)
        )
      }).done(function() {
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    },

    stopUpdating: function () {
      window.clearTimeout(this.timer);
      this.isUpdating = false;
    },

    startUpdating: function () {
      if (this.isUpdating) {
        return;
      }
      this.isUpdating = true;
      var self = this;
      this.timer = window.setInterval(function() {
        self.updateUrl();
        self.fetch({
          beforeSend: window.App.addAuth.bind(window.App)
        });
      }, this.interval);
    }

  });
}());




/*global window, Backbone, arangoHelper, _ */

window.arangoDocumentModel = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  urlRoot: "/_api/document",
  defaults: {
    _id: "",
    _rev: "",
    _key: ""
  },
  getSorted: function () {
    'use strict';
    var self = this;
    var keys = Object.keys(self.attributes).sort(function (l, r) {
      var l1 = arangoHelper.isSystemAttribute(l);
      var r1 = arangoHelper.isSystemAttribute(r);

      if (l1 !== r1) {
        if (l1) {
          return -1;
        }
        return 1;
      }

      return l < r ? -1 : 1;
    });

    var sorted = {};
    _.each(keys, function (k) {
      sorted[k] = self.attributes[k];
    });
    return sorted;
  }
});

/*global window, Backbone */

window.Statistics = Backbone.Model.extend({
  defaults: {
  },

  url: function() {
    'use strict';
    return "/_admin/statistics";
  }
});

/*global window, Backbone */

window.StatisticsDescription = Backbone.Model.extend({
  defaults: {
    "figures" : "",
    "groups" : ""
  },
  url: function() {
    'use strict';

    return "/_admin/statistics-description";
  }

});

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window, $, _ */
(function () {

  "use strict";

  window.PaginatedCollection = Backbone.Collection.extend({
    page: 0,
    pagesize: 10,
    totalAmount: 0,

    getPage: function() {
      return this.page + 1;
    },

    setPage: function(counter) {
      if (counter >= this.getLastPageNumber()) {
        this.page = this.getLastPageNumber()-1;
        return;
      }
      if (counter < 1) {
        this.page = 0;
        return;
      }
      this.page = counter - 1;

    },

    getLastPageNumber: function() {
      return Math.max(Math.ceil(this.totalAmount / this.pagesize), 1);
    },

    getOffset: function() {
      return this.page * this.pagesize;
    },

    getPageSize: function() {
      return this.pagesize;
    },

    setPageSize: function(newPagesize) {
      if (newPagesize === "all") {
        this.pagesize = 'all';
      }
      else {
        try {
          newPagesize = parseInt(newPagesize, 10);
          this.pagesize = newPagesize;
        }
        catch (ignore) {
        }
      }
    },

    setToFirst: function() {
      this.page = 0;
    },

    setToLast: function() {
      this.setPage(this.getLastPageNumber());
    },

    setToPrev: function() {
      this.setPage(this.getPage() - 1);

    },

    setToNext: function() {
      this.setPage(this.getPage() + 1);
    },

    setTotal: function(total) {
      this.totalAmount = total;
    },

    getTotal: function() {
      return this.totalAmount;
    },

    setTotalMinusOne: function() {
      this.totalAmount--;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window */
window.StatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,
  url: "/_admin/statistics"
});

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, arangoDocumentModel, _, arangoHelper, $*/
(function() {
  "use strict";

  window.arangoDocuments = window.PaginatedCollection.extend({
    collectionID: 1,

    filters: [],

    MAX_SORT: 12000,

    lastQuery: {},

    sortAttribute: "_key",

    url: '/_api/documents',
    model: window.arangoDocumentModel,

    loadTotal: function() {
      var self = this;
      $.ajax({
        cache: false,
        type: "GET",
        url: "/_api/collection/" + this.collectionID + "/count",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          self.setTotal(data.count);
        }
      });
    },

    setCollection: function(id) {
      this.resetFilter();
      this.collectionID = id;
      this.setPage(1);
      this.loadTotal();
    },

    setSort: function(key) {
      this.sortAttribute = key;
    },

    getSort: function() {
      return this.sortAttribute;
    },

    addFilter: function(attr, op, val) {
      this.filters.push({
        attr: attr,
        op: op,
        val: val
      });
    },

    setFiltersForQuery: function(bindVars) {
      if (this.filters.length === 0) {
        return "";
      }
      var query = " FILTER",
      parts = _.map(this.filters, function(f, i) {
        var res = " x.`";
        res += f.attr;
        res += "` ";
        res += f.op;
        res += " @param";
        res += i;
        bindVars["param" + i] = f.val;
        return res;
      });
      return query + parts.join(" &&");
    },

    setPagesize: function(size) {
      this.setPageSize(size);
    },

    resetFilter: function() {
      this.filters = [];
    },

    moveDocument: function (key, fromCollection, toCollection, callback) {
      var querySave, queryRemove, queryObj, bindVars = {
        "@collection": fromCollection,
        "filterid": key
      }, queryObj1, queryObj2;

      querySave = "FOR x IN @@collection";
      querySave += " FILTER x._key == @filterid";
      querySave += " INSERT x IN ";
      querySave += toCollection;

      queryRemove = "FOR x in @@collection";
      queryRemove += " FILTER x._key == @filterid";
      queryRemove += " REMOVE x IN @@collection";

      queryObj1 = {
        query: querySave,
        bindVars: bindVars
      };

      queryObj2 = {
        query: queryRemove,
        bindVars: bindVars
      };

      window.progressView.show();
      // first insert docs in toCollection
      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj1),
        contentType: "application/json",
        success: function(data) {
          // if successful remove unwanted docs
          $.ajax({
            cache: false,
            type: 'POST',
            async: true,
            url: '/_api/cursor',
            data: JSON.stringify(queryObj2),
            contentType: "application/json",
            success: function(data) {
              if (callback) {
                callback();
              }
              window.progressView.hide();
            },
            error: function(data) {
              window.progressView.hide();
              arangoHelper.arangoNotification(
                "Document error", "Documents inserted, but could not be removed."
              );
            }
          });
        },
        error: function(data) {
          window.progressView.hide();
          arangoHelper.arangoNotification("Document error", "Could not move selected documents.");
        }
      });
    },

    getDocuments: function (callback) {
      window.progressView.showWithDelay(300, "Fetching documents...");
      var self = this,
          query,
          bindVars,
          tmp,
          queryObj;
      bindVars = {
        "@collection": this.collectionID,
        "offset": this.getOffset(),
        "count": this.getPageSize()
      };

      // fetch just the first 25 attributes of the document
      // this number is arbitrary, but may reduce HTTP traffic a bit
      query = "FOR x IN @@collection LET att = SLICE(ATTRIBUTES(x), 0, 25)";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        if (this.getSort() === '_key') {
          query += " SORT TO_NUMBER(x." + this.getSort() + ") == 0 ? x."
                + this.getSort() + " : TO_NUMBER(x." + this.getSort() + ")";
        }
        else {
          query += " SORT x." + this.getSort();
        }
      }

      if (bindVars.count !== 'all') {
        query += " LIMIT @offset, @count RETURN KEEP(x, att)";
      }
      else {
        tmp = {
          "@collection": this.collectionID
        };
        bindVars = tmp;
        query += " RETURN KEEP(x, att)";
      }

      queryObj = {
        query: query,
        bindVars: bindVars
      };
      if (this.getTotal() < 10000 || this.filters.length > 0) {
        queryObj.options = {
          fullCount: true,
        };
      }

      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj),
        contentType: "application/json",
        success: function(data) {
          window.progressView.toShow = false;
          self.clearDocuments();
          if (data.extra && data.extra.stats.fullCount !== undefined) {
            self.setTotal(data.extra.stats.fullCount);
          }
          if (self.getTotal() !== 0) {
            _.each(data.result, function(v) {
              self.add({
                "id": v._id,
                "rev": v._rev,
                "key": v._key,
                "content": v
              });
            });
          }
          self.lastQuery = queryObj;
          callback();
          window.progressView.hide();
        },
        error: function(data) {
          window.progressView.hide();
          arangoHelper.arangoNotification("Document error", "Could not fetch requested documents.");
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    buildDownloadDocumentQuery: function() {
      var self = this, query, queryObj, bindVars;

      bindVars = {
        "@collection": this.collectionID
      };

      query = "FOR x in @@collection";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        query += " SORT x." + this.getSort();
      }

      query += " RETURN x";

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      return queryObj;
    },

    uploadDocuments : function (file) {
      var result;
      $.ajax({
        type: "POST",
        async: false,
        url:
        '/_api/import?type=auto&collection='+
        encodeURIComponent(this.collectionID)+
        '&createCollection=false',
        data: file,
        processData: false,
        contentType: 'json',
        dataType: 'json',
        complete: function(xhr) {
          if (xhr.readyState === 4 && xhr.status === 201) {
            result = true;
          } else {
            result = "Upload error";
          }

          try {
            var data = JSON.parse(xhr.responseText);
            if (data.errors > 0) {
              result = "At least one error occurred during upload";
            }
          }
          catch (err) {
          }               
        }
      });
      return result;
    }
  });
}());

/*jshint unused: false */
/*global EJS, window, _, $*/
(function() {
  "use strict";
  // For tests the templates are loaded some where else.
  // We need to use a different engine there.
  if (!window.hasOwnProperty("TEST_BUILD")) {
    var TemplateEngine = function() {
      var exports = {};
      exports.createTemplate = function(id) {
        var template = $("#" + id.replace(".", "\\.")).html();
        return {
          render: function(params) {
            return _.template(template, params);
          }
        };
      };
      return exports;
    };
    window.templateEngine = new TemplateEngine();
  }
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, arangoHelper, window*/

(function() {
  "use strict";
  window.FooterView = Backbone.View.extend({
    el: '#footerBar',
    system: {},
    isOffline: true,
    isOfflineCounter: 0,
    firstLogin: true,

    events: {
      'click .footer-center p' : 'showShortcutModal'
    },

    initialize: function () {
      //also server online check
      var self = this;
      window.setInterval(function(){
        self.getVersion();
      }, 15000);
      self.getVersion();
    },

    template: templateEngine.createTemplate("footerView.ejs"),

    showServerStatus: function(isOnline) {
      if (isOnline === true) {
        $('.serverStatusIndicator').addClass('isOnline');
        $('.serverStatusIndicator').addClass('fa-check-circle-o');
        $('.serverStatusIndicator').removeClass('fa-times-circle-o');
      }
      else {
        $('.serverStatusIndicator').removeClass('isOnline');
        $('.serverStatusIndicator').removeClass('fa-check-circle-o');
        $('.serverStatusIndicator').addClass('fa-times-circle-o');
      }
    },

    showShortcutModal: function() {
      window.arangoHelper.hotkeysFunctions.showHotkeysModal();
    },

    getVersion: function () {
      var self = this;

      // always retry this call, because it also checks if the server is online
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/version",
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          self.showServerStatus(true);
          if (self.isOffline === true) {
            self.isOffline = false;
            self.isOfflineCounter = 0;
            if (!self.firstLogin) {
              window.setTimeout(function(){
                self.showServerStatus(true);
              }, 1000);
            } else {
              self.firstLogin = false;
            }
            self.system.name = data.server;
            self.system.version = data.version;
            self.render();
          }
        },
        error: function (data) {
          self.isOffline = true;
          self.isOfflineCounter++;
          if (self.isOfflineCounter >= 1) {
            //arangoHelper.arangoError("Server", "Server is offline");
            self.showServerStatus(false);
          }
        }
      });

      if (! self.system.hasOwnProperty('database')) {
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/database/current",
          contentType: "application/json",
          processData: false,
          async: true,
          success: function(data) {
            var name = data.result.name;
            self.system.database = name;

            var timer = window.setInterval(function () {
              var navElement = $('#databaseNavi');

              if (navElement) {
                window.clearTimeout(timer);
                timer = null;

                if (name === '_system') {
                  // show "logs" button
                  $('.logs-menu').css('visibility', 'visible');
                  $('.logs-menu').css('display', 'inline');
                  // show dbs menues
                  $('#databaseNavi').css('display','inline');
                }
                else {
                  // hide "logs" button
                  $('.logs-menu').css('visibility', 'hidden');
                  $('.logs-menu').css('display', 'none');
                }
                self.render();
              }
            }, 50);
          }
        });
      }
    },

    renderVersion: function () {
      if (this.system.hasOwnProperty('database') && this.system.hasOwnProperty('name')) {
        $(this.el).html(this.template.render({
          name: this.system.name,
          version: this.system.version,
          database: this.system.database
        }));
      }
    },

    render: function () {
      if (!this.system.version) {
        this.getVersion();
      }
      $(this.el).html(this.template.render({
        name: this.system.name,
        version: this.system.version
      }));
      return this;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function () {
  "use strict";

  function fmtNumber (n, nk) {
    if (n === undefined || n === null) {
      n = 0;
    }

    return n.toFixed(nk);
  }

  window.DashboardView = Backbone.View.extend({
    el: '#content',
    interval: 10000, // in milliseconds
    defaultTimeFrame: 20 * 60 * 1000, // 20 minutes in milliseconds
    defaultDetailFrame: 2 * 24 * 60 * 60 * 1000,
    history: {},
    graphs: {},

    events: {
      // will be filled in initialize
    },

    tendencies: {
      asyncPerSecondCurrent: [
        "asyncPerSecondCurrent", "asyncPerSecondPercentChange"
      ],

      syncPerSecondCurrent: [
        "syncPerSecondCurrent", "syncPerSecondPercentChange"
      ],

      clientConnectionsCurrent: [
        "clientConnectionsCurrent", "clientConnectionsPercentChange"
      ],

      clientConnectionsAverage: [
        "clientConnections15M", "clientConnections15MPercentChange"
      ],

      numberOfThreadsCurrent: [
        "numberOfThreadsCurrent", "numberOfThreadsPercentChange"
      ],

      numberOfThreadsAverage: [
        "numberOfThreads15M", "numberOfThreads15MPercentChange"
      ],

      virtualSizeCurrent: [
        "virtualSizeCurrent", "virtualSizePercentChange"
      ],

      virtualSizeAverage: [
        "virtualSize15M", "virtualSize15MPercentChange"
      ]
    },

    barCharts: {
      totalTimeDistribution: [
        "queueTimeDistributionPercent", "requestTimeDistributionPercent"
      ],
      dataTransferDistribution: [
        "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"
      ]
    },

    barChartsElementNames: {
      queueTimeDistributionPercent: "Queue",
      requestTimeDistributionPercent: "Computation",
      bytesSentDistributionPercent: "Bytes sent",
      bytesReceivedDistributionPercent: "Bytes received"

    },

    getDetailFigure : function (e) {
      var figure = $(e.currentTarget).attr("id").replace(/ChartButton/g, "");
      return figure;
    },

    showDetail: function (e) {
      var self = this,
          figure = this.getDetailFigure(e),
          options;

      options = this.dygraphConfig.getDetailChartConfig(figure);

      this.getHistoryStatistics(figure);
      this.detailGraphFigure = figure;

      window.modalView.hideFooter = true;
      window.modalView.hide();
      window.modalView.show(
        "modalGraph.ejs",
        options.header,
        undefined,
        undefined,
        undefined,
        undefined,
        this.events
      );

      window.modalView.hideFooter = false;

      $('#modal-dialog').on('hidden', function () {
        self.hidden();
      });

      $('#modal-dialog').toggleClass("modal-chart-detail", true);

      options.height = $(window).height() * 0.7;
      options.width = $('.modal-inner-detail').width();

      // Reselect the labelsDiv. It was not known when requesting options
      options.labelsDiv = $(options.labelsDiv)[0];

      this.detailGraph = new Dygraph(
        document.getElementById("lineChartDetail"),
        this.history[this.server][figure],
        options
      );
    },

    hidden: function () {
      this.detailGraph.destroy();
      delete this.detailGraph;
      delete this.detailGraphFigure;
    },


    getCurrentSize: function (div) {
      if (div.substr(0,1) !== "#") {
        div = "#" + div;
      }
      var height, width;
      $(div).attr("style", "");
      height = $(div).height();
      width = $(div).width();
      return {
        height: height,
        width: width
      };
    },

    prepareDygraphs: function () {
      var self = this, options;
      this.dygraphConfig.getDashBoardFigures().forEach(function (f) {
        options = self.dygraphConfig.getDefaultConfig(f);
        var dimensions = self.getCurrentSize(options.div);
        options.height = dimensions.height;
        options.width = dimensions.width;
        self.graphs[f] = new Dygraph(
          document.getElementById(options.div),
          self.history[self.server][f] || [],
          options
        );
      });
    },

    initialize: function () {
      this.dygraphConfig = this.options.dygraphConfig;
      this.d3NotInitialised = true;
      this.events["click .dashboard-sub-bar-menu-sign"] = this.showDetail.bind(this);
      this.events["mousedown .dygraph-rangesel-zoomhandle"] = this.stopUpdating.bind(this);
      this.events["mouseup .dygraph-rangesel-zoomhandle"] = this.startUpdating.bind(this);

      this.server = this.options.serverToShow;

      if (! this.server) {
        this.server = "-local-";
      }

      this.history[this.server] = {};
    },

    updateCharts: function () {
      var self = this;
      if (this.detailGraph) {
        this.updateLineChart(this.detailGraphFigure, true);
        return;
      }
      this.prepareD3Charts(this.isUpdating);
      this.prepareResidentSize(this.isUpdating);
      this.updateTendencies();
      Object.keys(this.graphs).forEach(function (f) {
        self.updateLineChart(f, false);
      });
    },

    updateTendencies: function () {
      var self = this, map = this.tendencies;

      var tempColor = "";
      Object.keys(map).forEach(function (a) {
        var p = "";
        var v = 0;
        if (self.history.hasOwnProperty(self.server) &&
            self.history[self.server].hasOwnProperty(a)) {
          v = self.history[self.server][a][1];
        }

        if (v < 0) {
          tempColor = "red";
        }
        else {
          tempColor = "green";
          p = "+";
        }
        $("#" + a).html(self.history[self.server][a][0] + '<br/><span class="dashboard-figurePer" style="color: '
          + tempColor +';">' + p + v + '%</span>');
      });
    },


    updateDateWindow: function (graph, isDetailChart) {
      var t = new Date().getTime();
      var borderLeft, borderRight;
      if (isDetailChart && graph.dateWindow_) {
        borderLeft = graph.dateWindow_[0];
        borderRight = t - graph.dateWindow_[1] - this.interval * 5 > 0 ?
        graph.dateWindow_[1] : t;
        return [borderLeft, borderRight];
      }
      return [t - this.defaultTimeFrame, t];


    },

    updateLineChart: function (figure, isDetailChart) {
      var g = isDetailChart ? this.detailGraph : this.graphs[figure],
      opts = {
        file: this.history[this.server][figure],
        dateWindow: this.updateDateWindow(g, isDetailChart)
      };
      g.updateOptions(opts);
    },

    mergeDygraphHistory: function (newData, i) {
      var self = this, valueList;

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // need at least an empty history
        if (! self.history[self.server][f]) {
          self.history[self.server][f] = [];
        }

        // generate values for this key
        valueList = [];

        self.dygraphConfig.mapStatToFigure[f].forEach(function (a) {
          if (! newData[a]) {
            return;
          }

          if (a === "times") {
            valueList.push(new Date(newData[a][i] * 1000));
          }
          else {
            valueList.push(newData[a][i]);
          }
        });

        // if we found at list one value besides times, then use the entry
        if (valueList.length > 1) {
          self.history[self.server][f].push(valueList);
        }
      });
    },

    cutOffHistory: function (f, cutoff) {
      var self = this;

      while (self.history[self.server][f].length !== 0) {
        var v = self.history[self.server][f][0][0];

        if (v >= cutoff) {
          break;
        }

        self.history[self.server][f].shift();
      }
    },

    cutOffDygraphHistory: function (cutoff) {
      var self = this;
      var cutoffDate = new Date(cutoff);

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // history must be non-empty
        if (! self.history[self.server][f]) {
          return;
        }

        self.cutOffHistory(f, cutoffDate);
      });
    },

    mergeHistory: function (newData) {
      var self = this, i;

      for (i = 0; i < newData.times.length; ++i) {
        this.mergeDygraphHistory(newData, i);
      }

      this.cutOffDygraphHistory(new Date().getTime() - this.defaultTimeFrame);

      // convert tendency values
      Object.keys(this.tendencies).forEach(function (a) {
        var n1 = 1;
        var n2 = 1;

        if (a === "virtualSizeCurrent" || a === "virtualSizeAverage") {
          newData[self.tendencies[a][0]] /= (1024 * 1024 * 1024);
          n1 = 2;
        }
        else if (a === "clientConnectionsCurrent") {
          n1 = 0;
        }
        else if (a === "numberOfThreadsCurrent") {
          n1 = 0;
        }

        self.history[self.server][a] = [
          fmtNumber(newData[self.tendencies[a][0]], n1),
          fmtNumber(newData[self.tendencies[a][1]] * 100, n2)
        ];
      });

      // update distribution
      Object.keys(this.barCharts).forEach(function (a) {
        self.history[self.server][a] = self.mergeBarChartData(self.barCharts[a], newData);
      });

      // update physical memory
      self.history[self.server].physicalMemory = newData.physicalMemory;
      self.history[self.server].residentSizeCurrent = newData.residentSizeCurrent;
      self.history[self.server].residentSizePercent = newData.residentSizePercent;

      // generate chart description
      self.history[self.server].residentSizeChart =
      [
        {
          "key": "",
          "color": this.dygraphConfig.colors[1],
          "values": [
            {
              label: "used",
              value: newData.residentSizePercent * 100
            }
          ]
        },
        {
          "key": "",
          "color": this.dygraphConfig.colors[0],
          "values": [
            {
              label: "used",
              value: 100 - newData.residentSizePercent * 100
            }
          ]
        }
      ]
      ;

      // remember next start
      this.nextStart = newData.nextStart;
    },

    mergeBarChartData: function (attribList, newData) {
      var i, v1 = {
        "key": this.barChartsElementNames[attribList[0]],
        "color": this.dygraphConfig.colors[0],
        "values": []
      }, v2 = {
        "key": this.barChartsElementNames[attribList[1]],
        "color": this.dygraphConfig.colors[1],
        "values": []
      };
      for (i = newData[attribList[0]].values.length - 1;  0 <= i;  --i) {
        v1.values.push({
          label: this.getLabel(newData[attribList[0]].cuts, i),
          value: newData[attribList[0]].values[i]
        });
        v2.values.push({
          label: this.getLabel(newData[attribList[1]].cuts, i),
          value: newData[attribList[1]].values[i]
        });
      }
      return [v1, v2];
    },

    getLabel: function (cuts, counter) {
      if (!cuts[counter]) {
        return ">" + cuts[counter - 1];
      }
      return counter === 0 ? "0 - " +
                         cuts[counter] : cuts[counter - 1] + " - " + cuts[counter];
    },

    getStatistics: function (callback) {
      var self = this;
      var url = "/_db/_system/_admin/aardvark/statistics/short";
      var urlParams = "?start=";

      if (self.nextStart) {
        urlParams += self.nextStart;
      }
      else {
        urlParams += (new Date().getTime() - self.defaultTimeFrame) / 1000;
      }

      if (self.server !== "-local-") {
        url = self.server.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=short&DBserver=" + self.server.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        {async: true}
      ).done(
        function (d) {
          if (d.times.length > 0) {
            self.isUpdating = true;
            self.mergeHistory(d);
          }
          if (self.isUpdating === false) {
            return;
          }
          if (callback) {
            callback();
          }
          self.updateCharts();
      });
    },

    getHistoryStatistics: function (figure) {
      var self = this;
      var url = "statistics/long";

      var urlParams
        = "?filter=" + this.dygraphConfig.mapStatToFigure[figure].join();

      if (self.server !== "-local-") {
        url = self.server.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=long&DBserver=" + self.server.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        {async: true}
      ).done(
        function (d) {
          var i;

          self.history[self.server][figure] = [];

          for (i = 0;  i < d.times.length;  ++i) {
            self.mergeDygraphHistory(d, i, true);
          }
        }
      );
    },

    prepareResidentSize: function (update) {
      var self = this;

      var dimensions = this.getCurrentSize('#residentSizeChartContainer');

      var current = self.history[self.server].residentSizeCurrent / 1024 / 1024;
      var currentA = "";

      if (current < 1025) {
        currentA = fmtNumber(current, 2) + " MB";
      }
      else {
        currentA = fmtNumber(current / 1024, 2) + " GB";
      }

      var currentP = fmtNumber(self.history[self.server].residentSizePercent * 100, 2);
      var data = [fmtNumber(self.history[self.server].physicalMemory / 1024 / 1024 / 1024, 0) + " GB"];

      nv.addGraph(function () {
        var chart = nv.models.multiBarHorizontalChart()
          .x(function (d) {
            return d.label;
          })
          .y(function (d) {
            return d.value;
          })
          .width(dimensions.width)
          .height(dimensions.height)
          .margin({
            top: ($("residentSizeChartContainer").outerHeight() - $("residentSizeChartContainer").height()) / 2,
            right: 1,
            bottom: ($("residentSizeChartContainer").outerHeight() - $("residentSizeChartContainer").height()) / 2,
            left: 1
          })
          .showValues(false)
          .showYAxis(false)
          .showXAxis(false)
          .transitionDuration(100)
          .tooltips(false)
          .showLegend(false)
          .showControls(false)
          .stacked(true);

        chart.yAxis
          .tickFormat(function (d) {return d + "%";})
          .showMaxMin(false);
        chart.xAxis.showMaxMin(false);

        d3.select('#residentSizeChart svg')
          .datum(self.history[self.server].residentSizeChart)
          .call(chart);

        d3.select('#residentSizeChart svg').select('.nv-zeroLine').remove();

        if (update) {
          d3.select('#residentSizeChart svg').select('#total').remove();
          d3.select('#residentSizeChart svg').select('#percentage').remove();
        }

        d3.select('.dashboard-bar-chart-title .percentage')
          .html(currentA + " ("+ currentP + " %)");

        d3.select('.dashboard-bar-chart-title .absolut')
          .html(data[0]);

        nv.utils.windowResize(chart.update);

        return chart;
      }, function() {
        d3.selectAll("#residentSizeChart .nv-bar").on('click',
          function() {
            // no idea why this has to be empty, well anyways...
          }
        );
      });
    },

    prepareD3Charts: function (update) {
      var self = this;

      var barCharts = {
        totalTimeDistribution: [
          "queueTimeDistributionPercent", "requestTimeDistributionPercent"],
        dataTransferDistribution: [
          "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"]
      };

      if (this.d3NotInitialised) {
          update = false;
          this.d3NotInitialised = false;
      }

      _.each(Object.keys(barCharts), function (k) {
        var dimensions = self.getCurrentSize('#' + k
          + 'Container .dashboard-interior-chart');

        var selector = "#" + k + "Container svg";

        nv.addGraph(function () {
          var tickMarks = [0, 0.25, 0.5, 0.75, 1];
          var marginLeft = 75;
          var marginBottom = 23;
          var bottomSpacer = 6;

          if (dimensions.width < 219) {
            tickMarks = [0, 0.5, 1];
            marginLeft = 72;
            marginBottom = 21;
            bottomSpacer = 5;
          }
          else if (dimensions.width < 299) {
            tickMarks = [0, 0.3334, 0.6667, 1];
            marginLeft = 77;
          }
          else if (dimensions.width < 379) {
            marginLeft = 87;
          }
          else if (dimensions.width < 459) {
            marginLeft = 95;
          }
          else if (dimensions.width < 539) {
            marginLeft = 100;
          }
          else if (dimensions.width < 619) {
            marginLeft = 105;
          }

          var chart = nv.models.multiBarHorizontalChart()
            .x(function (d) {
              return d.label;
            })
            .y(function (d) {
              return d.value;
            })
            .width(dimensions.width)
            .height(dimensions.height)
            .margin({
              top: 5,
              right: 20,
              bottom: marginBottom,
              left: marginLeft
            })
            .showValues(false)
            .showYAxis(true)
            .showXAxis(true)
            .transitionDuration(100)
            .tooltips(false)
            .showLegend(false)
            .showControls(false)
            .forceY([0,1]);

          chart.yAxis
            .showMaxMin(false);

          var yTicks2 = d3.select('.nv-y.nv-axis')
            .selectAll('text')
            .attr('transform', 'translate (0, ' + bottomSpacer + ')') ;

          chart.yAxis
            .tickValues(tickMarks)
            .tickFormat(function (d) {return fmtNumber(((d * 100 * 100) / 100), 0) + "%";});

          d3.select(selector)
            .datum(self.history[self.server][k])
            .call(chart);

          nv.utils.windowResize(chart.update);

          return chart;
        }, function() {
          d3.selectAll(selector + " .nv-bar").on('click',
            function() {
              // no idea why this has to be empty, well anyways...
            }
          );
        });
      });

    },

    stopUpdating: function () {
      this.isUpdating = false;
    },

  startUpdating: function () {
    var self = this;
    if (self.timer) {
      return;
    }
    self.timer = window.setInterval(function () {
        self.getStatistics();
      },
      self.interval
    );
  },


  resize: function () {
    if (!this.isUpdating) {
      return;
    }
    var self = this, dimensions;
      _.each(this.graphs,function (g) {
      dimensions = self.getCurrentSize(g.maindiv_.id);
      g.resize(dimensions.width, dimensions.height);
    });
    if (this.detailGraph) {
      dimensions = this.getCurrentSize(this.detailGraph.maindiv_.id);
      this.detailGraph.resize(dimensions.width, dimensions.height);
    }
    this.prepareD3Charts(true);
    this.prepareResidentSize(true);
  },

  template: templateEngine.createTemplate("dashboardView.ejs"),

  render: function (modalView) {
    if (!modalView)  {
      $(this.el).html(this.template.render());
    }
    var callback = function() {
      this.prepareDygraphs();
      if (this.isUpdating) {
        this.prepareD3Charts();
        this.prepareResidentSize();
        this.updateTendencies();
      }
      this.startUpdating();
    }.bind(this);

    //check if user has _system permission
    var authorized = this.options.database.hasSystemAccess();
    if (!authorized) {
      $('.contentDiv').remove();
      $('.headerBar').remove();
      $('.dashboard-headerbar').remove();
      $('.dashboard-row').remove();
      $('#content').append(
        '<div style="color: red">You do not have permission to view this page.</div>'
      );
      $('#content').append(
        '<div style="color: red">You can switch to \'_system\' to see the dashboard.</div>'
      );
    }
    else {
      this.getStatistics(callback);
    }
  }
});
}());

/*jshint browser: true */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  var createButtonStub = function(type, title, cb, confirm) {
    return {
      type: type,
      title: title,
      callback: cb,
      confirm: confirm
    };
  };

  var createTextStub = function(type, label, value, info, placeholder, mandatory, joiObj,
                                addDelete, addAdd, maxEntrySize, tags) {
    var obj = {
      type: type,
      label: label
    };
    if (value !== undefined) {
      obj.value = value;
    }
    if (info !== undefined) {
      obj.info = info;
    }
    if (placeholder !== undefined) {
      obj.placeholder = placeholder;
    }
    if (mandatory !== undefined) {
      obj.mandatory = mandatory;
    }
    if (addDelete !== undefined) {
      obj.addDelete = addDelete;
    }
    if (addAdd !== undefined) {
      obj.addAdd = addAdd;
    }
    if (maxEntrySize !== undefined) {
      obj.maxEntrySize = maxEntrySize;
    }
    if (tags !== undefined) {
      obj.tags = tags;
    }
    if (joiObj){
      // returns true if the string contains the match
      obj.validateInput = function() {
        // return regexp.test(el.val());
        return joiObj;
      };
    }
    return obj;
  };

  window.ModalView = Backbone.View.extend({

    _validators: [],
    _validateWatchers: [],
    baseTemplate: templateEngine.createTemplate("modalBase.ejs"),
    tableTemplate: templateEngine.createTemplate("modalTable.ejs"),
    el: "#modalPlaceholder",
    contentEl: "#modalContent",
    hideFooter: false,
    confirm: {
      list: "#modal-delete-confirmation",
      yes: "#modal-confirm-delete",
      no: "#modal-abort-delete"
    },
    enabledHotkey: false,
    enableHotKeys : true,

    buttons: {
      SUCCESS: "success",
      NOTIFICATION: "notification",
      DELETE: "danger",
      NEUTRAL: "neutral",
      CLOSE: "close"
    },
    tables: {
      READONLY: "readonly",
      TEXT: "text",
      BLOB: "blob",
      PASSWORD: "password",
      SELECT: "select",
      SELECT2: "select2",
      CHECKBOX: "checkbox"
    },

    initialize: function() {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
    },

    createModalHotkeys: function() {
      //submit modal
      $(this.el).bind('keydown', 'return', function(){
        $('.modal-footer .button-success').click();
      });
      $("input", $(this.el)).bind('keydown', 'return', function(){
        $('.modal-footer .button-success').click();
      });
      $("select", $(this.el)).bind('keydown', 'return', function(){
        $('.modal-footer .button-success').click();
      });
    },

    createInitModalHotkeys: function() {
      var self = this;
      //navigate through modal buttons
      //left cursor
      $(this.el).bind('keydown', 'left', function(){
        self.navigateThroughButtons('left');
      });
      //right cursor
      $(this.el).bind('keydown', 'right', function(){
        self.navigateThroughButtons('right');
      });

    },

    navigateThroughButtons: function(direction) {
      var hasFocus = $('.modal-footer button').is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          $('.modal-footer button').first().focus();
        }
        else if (direction === 'right') {
          $('.modal-footer button').last().focus();
        }
      }
      else if (hasFocus === true) {
        if (direction === 'left') {
          $(':focus').prev().focus();
        }
        else if (direction === 'right') {
          $(':focus').next().focus();
        }
      }

    },

    createCloseButton: function(title, cb) {
      var self = this;
      return createButtonStub(this.buttons.CLOSE, title, function () {
        self.hide();
        if (cb) {
          cb();
        }
      });
    },

    createSuccessButton: function(title, cb) {
      return createButtonStub(this.buttons.SUCCESS, title, cb);
    },

    createNotificationButton: function(title, cb) {
      return createButtonStub(this.buttons.NOTIFICATION, title, cb);
    },

    createDeleteButton: function(title, cb, confirm) {
      return createButtonStub(this.buttons.DELETE, title, cb, confirm);
    },

    createNeutralButton: function(title, cb) {
      return createButtonStub(this.buttons.NEUTRAL, title, cb);
    },

    createDisabledButton: function(title) {
      var disabledButton = createButtonStub(this.buttons.NEUTRAL, title);
      disabledButton.disabled = true;
      return disabledButton;
    },

    createReadOnlyEntry: function(id, label, value, info, addDelete, addAdd) {
      var obj = createTextStub(this.tables.READONLY, label, value, info,undefined, undefined,
        undefined,addDelete, addAdd);
      obj.id = id;
      return obj;
    },

    createTextEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.TEXT, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createBlobEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.BLOB, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createSelect2Entry: function(
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags) {
      var obj = createTextStub(this.tables.SELECT2, label, value, info, placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags);
      obj.id = id;
      return obj;
    },

    createPasswordEntry: function(id, label, value, info, placeholder, mandatory) {
      var obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory);
      obj.id = id;
      return obj;
    },

    createCheckboxEntry: function(id, label, value, info, checked) {
      var obj = createTextStub(this.tables.CHECKBOX, label, value, info);
      obj.id = id;
      if (checked) {
        obj.checked = checked;
      }
      return obj;
    },

    createSelectEntry: function(id, label, selected, info, options) {
      var obj = createTextStub(this.tables.SELECT, label, null, info);
      obj.id = id;
      if (selected) {
        obj.selected = selected;
      }
      obj.options = options;
      return obj;
    },

    createOptionEntry: function(label, value) {
      return {
        label: label,
        value: value || label
      };
    },

    show: function(templateName, title, buttons, tableContent, advancedContent, extraInfo, events, noConfirm) {
      var self = this, lastBtn, confirmMsg, closeButtonFound = false;
      buttons = buttons || [];
      noConfirm = Boolean(noConfirm);
      this.clearValidators();
      if (buttons.length > 0) {
        buttons.forEach(function (b) {
          if (b.type === self.buttons.CLOSE) {
              closeButtonFound = true;
          }
          if (b.type === self.buttons.DELETE) {
              confirmMsg = confirmMsg || b.confirm;
          }
        });
        if (!closeButtonFound) {
          // Insert close as second from right
          lastBtn = buttons.pop();
          buttons.push(self.createCloseButton('Cancel'));
          buttons.push(lastBtn);
        }
      } else {
        buttons.push(self.createCloseButton('Dismiss'));
      }
      $(this.el).html(this.baseTemplate.render({
        title: title,
        buttons: buttons,
        hideFooter: this.hideFooter,
        confirm: confirmMsg
      }));
      _.each(buttons, function(b, i) {
        if (b.disabled || !b.callback) {
          return;
        }
        if (b.type === self.buttons.DELETE && !noConfirm) {
          $("#modalButton" + i).bind("click", function() {
            $(self.confirm.yes).unbind("click");
            $(self.confirm.yes).bind("click", b.callback);
            $(self.confirm.list).css("display", "block");
          });
          return;
        }
        $("#modalButton" + i).bind("click", b.callback);
      });
      $(this.confirm.no).bind("click", function() {
        $(self.confirm.list).css("display", "none");
      });

      var template = templateEngine.createTemplate(templateName);
      $(".modal-body").html(template.render({
        content: tableContent,
        advancedContent: advancedContent,
        info: extraInfo
      }));
      $('.modalTooltips').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });

      var completeTableContent = tableContent || [];
      if (advancedContent && advancedContent.content) {
        completeTableContent = completeTableContent.concat(advancedContent.content);
      }

      _.each(completeTableContent, function(row) {
        self.modalBindValidation(row);
        if (row.type === self.tables.SELECT2) {
          //handle select2
          $('#'+row.id).select2({
            tags: row.tags || [],
            showSearchBox: false,
            minimumResultsForSearch: -1,
            width: "336px",
            maximumSelectionSize: row.maxEntrySize || 8
          });
        }
      });

      if (events) {
        this.events = events;
        this.delegateEvents();
      }

      $("#modal-dialog").modal("show");

      //enable modal hotkeys after rendering is complete
      if (this.enabledHotkey === false) {
        this.createInitModalHotkeys();
        this.enabledHotkey = true;
      }
      if (this.enableHotKeys) {
        this.createModalHotkeys();
      }

      //if input-field is available -> autofocus first one
      var focus = $('#modal-dialog').find('input');
      if (focus) {
        setTimeout(function() {
          var focus = $('#modal-dialog');
          if (focus.length > 0) {
            focus = focus.find('input'); 
              if (focus.length > 0) {
                $(focus[0]).focus();
              }
          }
        }, 800);
      }

    },

    modalBindValidation: function(entry) {
      var self = this;
      if (entry.hasOwnProperty("id")
        && entry.hasOwnProperty("validateInput")) {
        var validCheck = function() {
          var $el = $("#" + entry.id);
          var validation = entry.validateInput($el);
          var error = false;
          _.each(validation, function(validator) {
            var value = $el.val();
            if (!validator.rule) {
              validator = {rule: validator};
            }
            if (typeof validator.rule === 'function') {
              try {
                validator.rule(value);
              } catch (e) {
                error = validator.msg || e.message;
              }
            } else {
              var result = Joi.validate(value, validator.rule);
              if (result.error) {
                error = validator.msg || result.error.message;
              }
            }
            if (error) {
              return false;
            }
          });
          if (error) {
            return error;
          }
        };
        var $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function() {
          var msg = validCheck();
          var errorElement = $el.next()[0];
          if (msg) {
            $el.addClass('invalid-input');
            if (errorElement) {
              //error element available
              $(errorElement).text(msg);
            }
            else {
              //error element not available
              $el.after('<p class="errorMessage">' + msg+ '</p>');
            }
            $('.modal-footer .button-success')
              .prop('disabled', true)
              .addClass('disabled');
          } else {
            $el.removeClass('invalid-input');
            if (errorElement) {
              $(errorElement).remove();
            }
            self.modalTestAll();
          }
        });
        this._validators.push(validCheck);
        this._validateWatchers.push($el);
      }
      
    },

    modalTestAll: function() {
      var tests = _.map(this._validators, function(v) {
        return v();
      });
      var invalid = _.any(tests);
      if (invalid) {
        $('.modal-footer .button-success')
          .prop('disabled', true)
          .addClass('disabled');
      } else {
        $('.modal-footer .button-success')
          .prop('disabled', false)
          .removeClass('disabled');
      }
      return !invalid;
    },

    clearValidators: function() {
      this._validators = [];
      _.each(this._validateWatchers, function(w) {
        w.unbind('keyup focusout');
      });
      this._validateWatchers = [];
    },

    hide: function() {
      this.clearValidators();
      $("#modal-dialog").modal("hide");
    }
  });
}());

/*global _, Dygraph, window, document */

(function () {
  "use strict";
  window.dygraphConfig = {
    defaultFrame : 20 * 60 * 1000,

    zeropad: function (x) {
      if (x < 10) {
        return "0" + x;
      }
      return x;
    },

    xAxisFormat: function (d) {
      if (d === -1) {
        return "";
      }
      var date = new Date(d);
      return this.zeropad(date.getHours()) + ":"
        + this.zeropad(date.getMinutes()) + ":"
        + this.zeropad(date.getSeconds());
    },

    mergeObjects: function (o1, o2, mergeAttribList) {
      if (!mergeAttribList) {
        mergeAttribList = [];
      }
      var vals = {}, res;
      mergeAttribList.forEach(function (a) {
        var valO1 = o1[a],
        valO2 = o2[a];
        if (valO1 === undefined) {
          valO1 = {};
        }
        if (valO2 === undefined) {
          valO2 = {};
        }
        vals[a] = _.extend(valO1, valO2);
      });
      res = _.extend(o1, o2);
      Object.keys(vals).forEach(function (k) {
        res[k] = vals[k];
      });
      return res;
    },

    mapStatToFigure : {
      residentSize : ["times", "residentSizePercent"],
      pageFaults : ["times", "majorPageFaultsPerSecond", "minorPageFaultsPerSecond"],
      systemUserTime : ["times", "systemTimePerSecond", "userTimePerSecond"],
      totalTime : ["times", "avgQueueTime", "avgRequestTime", "avgIoTime"],
      dataTransfer : ["times", "bytesSentPerSecond", "bytesReceivedPerSecond"],
      requests : ["times", "getsPerSecond", "putsPerSecond", "postsPerSecond",
                  "deletesPerSecond", "patchesPerSecond", "headsPerSecond",
                  "optionsPerSecond", "othersPerSecond"]
    },

    //colors for dygraphs
    colors: ["#617e2b", "#296e9c", "#81ccd8", "#7ca530", "#3c3c3c",
             "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"],


    // figure dependend options
    figureDependedOptions: {
      clusterRequestsPerSecond: {
        showLabelsOnHighlight: true,
        title: '',
        header : "Cluster Requests per Second",
        stackedGraph: true,
        div: "lineGraphLegend",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }

              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      residentSize: {
        header: "Resident Size",
        axes: {
          y: {
            labelsKMG2: false,
            axisLabelFormatter: function (y) {
              return parseFloat(y.toPrecision(3) * 100) + "%";
            },
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3) * 100) + "%";
            }
          }
        }
      },

      pageFaults: {
        header : "Page Faults",
        visibility: [true, false],
        labels: ["datetime", "Major Page", "Minor Page"],
        div: "pageFaultsChart",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      systemUserTime: {
        div: "systemUserTimeChart",
        header: "System and User Time",
        labels: ["datetime", "System Time", "User Time"],
        stackedGraph: true,
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      totalTime: {
        div: "totalTimeChart",
        header: "Total Time",
        labels: ["datetime", "Queue", "Computation", "I/O"],
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        },
        stackedGraph: true
      },

      dataTransfer: {
        header: "Data Transfer",
        labels: ["datetime", "Bytes sent", "Bytes received"],
        stackedGraph: true,
        div: "dataTransferChart"
      },

      requests: {
        header: "Requests",
        labels: ["datetime", "GET", "PUT", "POST", "DELETE", "PATCH", "HEAD", "OPTIONS", "OTHER"],
        stackedGraph: true,
        div: "requestsChart",
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      }
    },

    getDashBoardFigures : function (all) {
      var result = [], self = this;
      Object.keys(this.figureDependedOptions).forEach(function (k) {
        // ClusterRequestsPerSecond should not be ignored. Quick Fix
        if (k !== "clusterRequestsPerSecond" && (self.figureDependedOptions[k].div || all)) {
          result.push(k);
        }
      });
      return result;
    },

    //configuration for chart overview
    getDefaultConfig: function (figure) {
      var self = this;
      var result = {
        digitsAfterDecimal: 1,
        drawGapPoints: true,
        fillGraph: true,
        showLabelsOnHighlight: false,
        strokeWidth: 1.5,
        strokeBorderWidth: 1.5,
        includeZero: true,
        highlightCircleSize: 2.5,
        labelsSeparateLines : true,
        strokeBorderColor: '#ffffff',
        interactionModel: {},
        maxNumberWidth : 10,
        colors: [this.colors[0]],
        xAxisLabelWidth: "50",
        rightGap: 15,
        showRangeSelector: false,
        rangeSelectorHeight: 50,
        rangeSelectorPlotStrokeColor: '#365300',
        rangeSelectorPlotFillColor: '',
        // rangeSelectorPlotFillColor: '#414a4c',
        pixelsPerLabel: 50,
        labelsKMG2: true,
        dateWindow: [
          new Date().getTime() -
            this.defaultFrame,
          new Date().getTime()
        ],
        axes: {
          x: {
            valueFormatter: function (d) {
              return self.xAxisFormat(d);
            }
          },
          y: {
            ticker: Dygraph.numericLinearTicks
          }
        }
      };
      if (this.figureDependedOptions[figure]) {
        result = this.mergeObjects(
          result, this.figureDependedOptions[figure], ["axes"]
        );
        if (result.div && result.labels) {
          result.colors = this.getColors(result.labels);
          result.labelsDiv = document.getElementById(result.div + "Legend");
          result.legend = "always";
          result.showLabelsOnHighlight = true;
        }
      }
      return result;

    },

    getDetailChartConfig: function (figure) {
      var result = _.extend(
        this.getDefaultConfig(figure),
        {
          showRangeSelector: true,
          interactionModel: null,
          showLabelsOnHighlight: true,
          highlightCircleSize: 2.5,
          legend: "always",
          labelsDiv: "div#detailLegend.dashboard-legend-inner"
        }
      );
      if (figure === "pageFaults") {
        result.visibility = [true, true];
      }
      if (!result.labels) {
        result.labels = ["datetime", result.header];
        result.colors = this.getColors(result.labels);
      }
      return result;
    },

    getColors: function (labels) {
      var colorList;
      colorList = this.colors.concat([]);
      return colorList.slice(0, labels.length - 1);
    }
  };
}());

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, window */
window.StatisticsDescriptionCollection = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: "/_admin/statistics-description",
  parse: function(response) {
    return response;
  }
});

/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert */

(function() {
  "use strict";

  window.ClusterDownView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterDown.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #relaunchCluster"  : "relaunchCluster",
      "click #upgradeCluster"   : "upgradeCluster",
      "click #editPlan"         : "editPlan",
      "click #submitEditPlan"   : "submitEditPlan",
      "click #deletePlan"       : "deletePlan",
      "click #submitDeletePlan" : "submitDeletePlan"
    },

    render: function() {
      var planVersion = window.versionHelper.fromString(
        window.App.clusterPlan.getVersion()
      );
      var currentVersion;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_admin/database/target-version",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          currentVersion = data.version;
        }
      });
      currentVersion = window.versionHelper.fromString(
        currentVersion
      );
      var shouldUpgrade = false;
      if (currentVersion.major > planVersion.major
        || (
          currentVersion.major === planVersion.major
          && currentVersion.minor > planVersion.minor
        )) {
        shouldUpgrade = true;
      }
      $(this.el).html(this.template.render({
        canUpgrade: shouldUpgrade
      }));
      $(this.el).append(this.modal.render({}));
    },

    relaunchCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be relaunched');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/relaunch",
        success: function() {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },

    upgradeCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be upgraded');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/upgrade",
        success: function() {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.clusterPlan.fetch();
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },

    editPlan: function() {
      $('#deletePlanModal').modal('hide');
      $('#editPlanModal').modal('show');
    },

    submitEditPlan : function() {
      $('#editPlanModal').modal('hide');
      window.App.clusterPlan.cleanUp();
      var plan = window.App.clusterPlan;
      if (plan.isTestSetup()) {
        window.App.navigate("planTest", {trigger : true});
        return;
      }
      window.App.navigate("planAsymmetrical", {trigger : true});
    },

    deletePlan: function() {
      $('#editPlanModal').modal('hide');
      $('#deletePlanModal').modal('show');
    },

    submitDeletePlan : function() {
      $('#deletePlanModal').modal('hide');
      window.App.clusterPlan.cleanUp();
      window.App.clusterPlan.destroy();
      window.App.clusterPlan = new window.ClusterPlan();
      window.App.planScenario();
    }

  });

}());

/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert, _ */

(function() {
  "use strict";

  window.ClusterUnreachableView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterUnreachable.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #clusterShutdown": "shutdown"
    },

    initialize: function() {
      this.coordinators = new window.ClusterCoordinators([], {
      });
    },


    retryConnection: function() {
      this.coordinators.checkConnection(function() {
        window.App.showCluster();
      });
    },

    shutdown: function() {
      window.clearTimeout(this.timer);
      window.App.shutdownView.clusterShutdown();
    },

    render: function() {
      var plan = window.App.clusterPlan;
      var list = [];
      if (plan && plan.has("runInfo")) {
        var startServerInfos = _.where(plan.get("runInfo"), {isStartServers: true});
        _.each(
          _.filter(startServerInfos, function(s) {
            return _.contains(s.roles, "Coordinator");
          }), function(s) {
            var name = s.endpoints[0].split("://")[1];
            name = name.split(":")[0];
            list.push(name);
          }
        );
      }
      $(this.el).html(this.template.render({
        coordinators: list
      }));
      $(this.el).append(this.modal.render({}));
      this.timer = window.setTimeout(this.retryConnection.bind(this), 10000);
    }

  });
}());

/*global Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.ServerDashboardView = window.DashboardView.extend({
    modal : true,

    hide: function() {
      window.App.showClusterView.startUpdating();
      this.stopUpdating();
    },

    render: function() {
      var self = this;
      window.modalView.hideFooter = true;
      window.modalView.show(
            "dashboardView.ejs",
            null,
            undefined,
            undefined,
            undefined,
            this.events

      );
      $('#modal-dialog').toggleClass("modal-chart-detail", true);
      window.DashboardView.prototype.render.bind(this)(true);
      window.modalView.hideFooter = false;


      $('#modal-dialog').on('hidden', function () {
            self.hide();
      });

      // Inject the closing x
      var closingX = document.createElement("button");
      closingX.className = "close";
      closingX.appendChild(
        document.createTextNode("×")
      );
      closingX = $(closingX);
      closingX.attr("data-dismiss", "modal");
      closingX.attr("aria-hidden", "true");
      closingX.attr("type", "button");
      $(".modal-body .headerBar:first-child")
        .toggleClass("headerBar", false)
        .toggleClass("modal-dashboard-header", true)
        .append(closingX);

    }
  });

}());

/*global templateEngine, window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";

  window.LoginModalView = Backbone.View.extend({

    template: templateEngine.createTemplate("loginModal.ejs"),
    el: '#modalPlaceholder',

    events: {
      "click #confirmLogin": "confirmLogin",
      "hidden #loginModalLayer": "hidden"
    },

    hidden: function () {
      this.undelegateEvents();
      window.App.isCheckingUser = false;
      $(this.el).html("");
    },

    confirmLogin: function() {
      var uName = $("#username").val();
      var passwd = $("#password").val();
      window.App.clusterPlan.storeCredentials(uName, passwd);
      this.hideModal();
    },

    hideModal: function () {
      $('#loginModalLayer').modal('hide');
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      $('#loginModalLayer').modal('show');
    }
  });

}());

/*global Backbone, $, _, window, templateEngine */
(function() {

  "use strict";

  window.PlanScenarioSelectorView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


    events: {
      "click #multiServerAsymmetrical": "multiServerAsymmetrical",
      "click #singleServer": "singleServer"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    },

    multiServerAsymmetrical: function() {
      window.App.navigate(
        "planAsymmetrical", {trigger: true}
      );
    },

    singleServer: function() {
      window.App.navigate(
        "planTest", {trigger: true}
      );
    }

  });
}());

/*global window, btoa, $, Backbone, templateEngine, alert, _ */

(function() {
  "use strict";

  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: templateEngine.createTemplate("symmetricPlan.ejs"),
    entryTemplate: templateEngine.createTemplate("serverEntry.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    connectionValidationKey: null,

    events: {
      "click #startSymmetricPlan"   : "startPlan",
      "click .add"                  : "addEntry",
      "click .delete"               : "removeEntry",
      "click #cancel"               : "cancel",
      "click #test-all-connections" : "checkAllConnections",
      "focusout .host"              : "checkAllConnections",
      "focusout .port"              : "checkAllConnections",
      "focusout .user"              : "checkAllConnections",
      "focusout .passwd"            : "checkAllConnections"
    },

    cancel: function() {
      if(window.App.clusterPlan.get("plan")) {
        window.App.navigate("handleClusterDown", {trigger: true});
      } else {
        window.App.navigate("planScenario", {trigger: true});
      }
    },

    startPlan: function() {
      var self = this;
      var data = {dispatchers: []};
      var foundCoordinator = false;
      var foundDBServer = false;
            data.useSSLonDBservers = !!$(".useSSLonDBservers").prop('checked');
      data.useSSLonCoordinators = !!$(".useSSLonCoordinators").prop('checked');
      $(".dispatcher").each(function(i, dispatcher) {
        var host = $(".host", dispatcher).val();
        var port = $(".port", dispatcher).val();
        var user = $(".user", dispatcher).val();
        var passwd = $(".passwd", dispatcher).val();
        if (!host || 0 === host.length || !port || 0 === port.length) {
          return true;
        }
        var hostObject = {host :  host + ":" + port};
        if (!self.isSymmetric) {
          hostObject.isDBServer = !!$(".isDBServer", dispatcher).prop('checked');
          hostObject.isCoordinator = !!$(".isCoordinator", dispatcher).prop('checked');
        } else {
          hostObject.isDBServer = true;
          hostObject.isCoordinator = true;
        }

        hostObject.username = user;
        hostObject.passwd = passwd;

        foundCoordinator = foundCoordinator || hostObject.isCoordinator;
        foundDBServer = foundDBServer || hostObject.isDBServer;

        data.dispatchers.push(hostObject);
      });
            if (!self.isSymmetric) {
        if (!foundDBServer) {
            alert("Please provide at least one database server");
            return;
        }
        if (!foundCoordinator) {
            alert("Please provide at least one coordinator");
            return;
        }
      } else {
        if ( data.dispatchers.length === 0) {
            alert("Please provide at least one host");
            return;
        }

      }

      data.type = this.isSymmetric ? "symmetricalSetup" : "asymmetricalSetup";
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      delete window.App.clusterPlan._coord;
      window.App.clusterPlan.save(
        data,
        {
          success : function() {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            window.App.updateAllUrls();
            window.App.navigate("showCluster", {trigger: true});
          },
          error: function(obj, err) {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            alert("Error while starting the cluster: " + err.statusText);
          }
        }
      );
          },

    addEntry: function() {
      //disable launch button
      this.disableLaunchButton();

      var lastUser = $("#server_list div.control-group.dispatcher:last .user").val();
      var lastPasswd = $("#server_list div.control-group.dispatcher:last .passwd").val();

      $("#server_list").append(this.entryTemplate.render({
        isSymmetric: this.isSymmetric,
        isFirst: false,
        isCoordinator: true,
        isDBServer: true,
        host: '',
        port: '',
        user: lastUser,
        passwd: lastPasswd
      }));
    },

    removeEntry: function(e) {
      $(e.currentTarget).closest(".control-group").remove();
      this.checkAllConnections();
    },

    render: function(isSymmetric) {
      var params = {},
        isFirst = true,
        config = window.App.clusterPlan.get("config");
      this.isSymmetric = isSymmetric;
      $(this.el).html(this.template.render({
        isSymmetric : isSymmetric,
        params      : params,
        useSSLonDBservers: config && config.useSSLonDBservers ?
            config.useSSLonDBservers : false,
        useSSLonCoordinators: config && config.useSSLonCoordinators ?
            config.useSSLonCoordinators : false
      }));
      if (config) {
        var self = this,
        isCoordinator = false,
        isDBServer = false;
        _.each(config.dispatchers, function(dispatcher) {
          if (dispatcher.allowDBservers === undefined) {
            isDBServer = true;
          } else {
            isDBServer = dispatcher.allowDBservers;
          }
          if (dispatcher.allowCoordinators === undefined) {
            isCoordinator = true;
          } else {
            isCoordinator = dispatcher.allowCoordinators;
          }
          var host = dispatcher.endpoint;
          host = host.split("//")[1];
          host = host.split(":");

          if (host === 'localhost') {
            host = '127.0.0.1';
          }

          var user = dispatcher.username;
          var passwd = dispatcher.passwd;
          var template = self.entryTemplate.render({
            isSymmetric: isSymmetric,
            isFirst: isFirst,
            host: host[0],
            port: host[1],
            isCoordinator: isCoordinator,
            isDBServer: isDBServer,
            user: user,
            passwd: passwd
          });
          $("#server_list").append(template);
          isFirst = false;
        });
      } else {
        $("#server_list").append(this.entryTemplate.render({
          isSymmetric: isSymmetric,
          isFirst: true,
          isCoordinator: true,
          isDBServer: true,
          host: '',
          port: '',
          user: '',
          passwd: ''
        }));
      }
      //initially disable lunch button
      this.disableLaunchButton();

      $(this.el).append(this.modal.render({}));

    },

    readAllConnections: function() {
      var res = [];
      $(".dispatcher").each(function(key, row) {
        var obj = {
          host: $('.host', row).val(),
          port: $('.port', row).val(),
          user: $('.user', row).val(),
          passwd: $('.passwd', row).val()
        };
        if (obj.host && obj.port) {
          res.push(obj);
        }
      });
      return res;
    },

    checkAllConnections: function() {
      var self = this;
      var connectionValidationKey = Math.random();
      this.connectionValidationKey = connectionValidationKey;
      $('.cluster-connection-check-success').remove();
      $('.cluster-connection-check-fail').remove();
      var list = this.readAllConnections();
      if (list.length) {
        try {
          $.ajax({
            async: true,
            cache: false,
            type: "POST",
            url: "/_admin/aardvark/cluster/communicationCheck",
            data: JSON.stringify(list),
            success: function(checkList) {
              if (connectionValidationKey === self.connectionValidationKey) {
                var dispatcher = $(".dispatcher");
                var i = 0;
                dispatcher.each(function(key, row) {
                  var host = $(".host", row).val();
                  var port = $(".port", row).val();
                  if (host && port) {
                    if (checkList[i]) {
                      $(".controls:first", row).append(
                        '<span class="cluster-connection-check-success">Connection: ok</span>'
                      );
                    } else {
                      $(".controls:first", row).append(
                        '<span class="cluster-connection-check-fail">Connection: fail</span>'
                      );
                    }
                    i++;
                  }
                });
                self.checkDispatcherArray(checkList, connectionValidationKey);
              }
            }
          });
        } catch (e) {
          this.disableLaunchButton();
        }
      }
    },

    checkDispatcherArray: function(dispatcherArray, connectionValidationKey) {
      if(
        (_.every(dispatcherArray, function (e) {return e;}))
          && connectionValidationKey === this.connectionValidationKey
        ) {
        this.enableLaunchButton();
      }
    },

    disableLaunchButton: function() {
      $('#startSymmetricPlan').attr('disabled', 'disabled');
      $('#startSymmetricPlan').removeClass('button-success');
      $('#startSymmetricPlan').addClass('button-neutral');
    },

    enableLaunchButton: function() {
      $('#startSymmetricPlan').attr('disabled', false);
      $('#startSymmetricPlan').removeClass('button-neutral');
      $('#startSymmetricPlan').addClass('button-success');
    }

  });

}());



/*global window, $, Backbone, templateEngine, alert */

(function() {
  "use strict";

  window.PlanTestView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("testPlan.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #startTestPlan": "startPlan",
      "click #cancel": "cancel"
    },

    cancel: function() {
      if(window.App.clusterPlan.get("plan")) {
        window.App.navigate("handleClusterDown", {trigger: true});
      } else {
        window.App.navigate("planScenario", {trigger: true});
      }
    },

    startPlan: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      var h = $("#host").val(),
        p = $("#port").val(),
        c = $("#coordinators").val(),
        d = $("#dbs").val();
      if (!h) {
        alert("Please define a host");
        return;
      }
      if (!p) {
        alert("Please define a port");
        return;
      }
      if (!c || c < 0) {
        alert("Please define a number of coordinators");
        return;
      }
      if (!d || d < 0) {
        alert("Please define a number of database servers");
        return;
      }
      delete window.App.clusterPlan._coord;
      window.App.clusterPlan.save(
        {
          type: "testSetup",
          dispatchers: h + ":" + p,
          numberDBServers: parseInt(d, 10),
          numberCoordinators: parseInt(c, 10)
        },
        {
          success: function() {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            window.App.updateAllUrls();
            window.App.navigate("showCluster", {trigger: true});
          },
          error: function(obj, err) {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            alert("Error while starting the cluster: " + err.statusText);
          }
        }
      );
    },

    render: function() {
      var param = {};
      var config = window.App.clusterPlan.get("config");
      if (config) {
        param.dbs = config.numberOfDBservers;
        param.coords = config.numberOfCoordinators;
        var host = config.dispatchers.d1.endpoint;
        host = host.split("://")[1];
        host = host.split(":");

        if (host === 'localhost') {
          host = '127.0.0.1';
        }

        param.hostname = host[0];
        param.port = host[1];
      } else {
        param.dbs = 3;
        param.coords = 2;
        param.hostname = window.location.hostname;

        if (param.hostname === 'localhost') {
          param.hostname = '127.0.0.1';
        }

        param.port = window.location.port;
      }
      $(this.el).html(this.template.render(param));
      $(this.el).append(this.modal.render({}));
    }
  });

}());

/*global window, $, Backbone, templateEngine, alert, _, d3, Dygraph, document */

(function() {
  "use strict";

  window.ShowClusterView = Backbone.View.extend({
    detailEl: '#modalPlaceholder',
    el: "#content",
    defaultFrame: 20 * 60 * 1000,
    template: templateEngine.createTemplate("showCluster.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    detailTemplate: templateEngine.createTemplate("detailView.ejs"),

    events: {
      "change #selectDB"                : "updateCollections",
      "change #selectCol"               : "updateShards",
      "click .dbserver.success"         : "dashboard",
      "click .coordinator.success"      : "dashboard"
    },

    replaceSVGs: function() {
      $(".svgToReplace").each(function() {
        var img = $(this);
        var id = img.attr("id");
        var src = img.attr("src");
        $.get(src, function(d) {
          var svg = $(d).find("svg");
          svg.attr("id", id)
          .attr("class", "icon")
          .removeAttr("xmlns:a");
          img.replaceWith(svg);
        }, "xml");
      });
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    },

    setShowAll: function() {
      this.graphShowAll = true;
    },

    resetShowAll: function() {
      this.graphShowAll = false;
      this.renderLineChart();
    },


    initialize: function() {
      this.interval = 10000;
      this.isUpdating = false;
      this.timer = null;
      this.knownServers = [];
      this.graph = undefined;
      this.graphShowAll = false;
      this.updateServerTime();
      this.dygraphConfig = this.options.dygraphConfig;
      this.dbservers = new window.ClusterServers([], {
        interval: this.interval
      });
      this.coordinators = new window.ClusterCoordinators([], {
        interval: this.interval
      });
      this.documentStore =  new window.arangoDocuments();
      this.statisticsDescription = new window.StatisticsDescription();
      this.statisticsDescription.fetch({
        async: false
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: this.interval
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards();
      this.startUpdating();
    },

    listByAddress: function(callback) {
      var byAddress = {};
      var self = this;
      this.dbservers.byAddress(byAddress, function(res) {
        self.coordinators.byAddress(res, callback);
      });
    },

    updateCollections: function() {
      var self = this;
      var selCol = $("#selectCol");
      var dbName = $("#selectDB").find(":selected").attr("id");
      if (!dbName) {
        return;
      }
      var colName = selCol.find(":selected").attr("id");
      selCol.html("");
      this.cols.getList(dbName, function(list) {
        _.each(_.pluck(list, "name"), function(c) {
          selCol.append("<option id=\"" + c + "\">" + c + "</option>");
        });
        var colToSel = $("#" + colName, selCol);
        if (colToSel.length === 1) {
          colToSel.prop("selected", true);
        }
        self.updateShards();
      });
    },

    updateShards: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");
      this.shards.getList(dbName, colName, function(list) {
        $(".shardCounter").html("0");
        _.each(list, function(s) {
          $("#" + s.server + "Shards").html(s.shards.length);
        });
      });
    },

    updateServerStatus: function(nextStep) {
      var self = this;
      var callBack = function(cls, stat, serv) {
        var id = serv,
          type, icon;
        id = id.replace(/\./g,'-');
        id = id.replace(/\:/g,'_');
        icon = $("#id" + id);
        if (icon.length < 1) {
          // callback after view was unrendered
          return;
        }
        type = icon.attr("class").split(/\s+/)[1];
        icon.attr("class", cls + " " + type + " " + stat);
        if (cls === "coordinator") {
          if (stat === "success") {
            $(".button-gui", icon.closest(".tile")).toggleClass("button-gui-disabled", false);
          } else {
            $(".button-gui", icon.closest(".tile")).toggleClass("button-gui-disabled", true);
          }

        }
      };
      this.coordinators.getStatuses(callBack.bind(this, "coordinator"), function() {
        self.dbservers.getStatuses(callBack.bind(self, "dbserver"));
        nextStep();
      });
    },

    updateDBDetailList: function() {
      var self = this;
      var selDB = $("#selectDB");
      var dbName = selDB.find(":selected").attr("id");
      selDB.html("");
      this.dbs.getList(function(dbList) {
        _.each(_.pluck(dbList, "name"), function(c) {
          selDB.append("<option id=\"" + c + "\">" + c + "</option>");
        });
        var dbToSel = $("#" + dbName, selDB);
        if (dbToSel.length === 1) {
          dbToSel.prop("selected", true);
        }
        self.updateCollections();
      });
    },

    rerender : function() {
      var self = this;
      this.updateServerStatus(function() {
        self.getServerStatistics(function() {
          self.updateServerTime();
          self.data = self.generatePieData();
          self.renderPieChart(self.data);
          self.renderLineChart();
          self.updateDBDetailList();
        });
      });
    },

    render: function() {
      this.knownServers = [];
      delete this.hist;
      var self = this;
      this.listByAddress(function(byAddress) {
        if (Object.keys(byAddress).length === 1) {
          self.type = "testPlan";
        } else {
          self.type = "other";
        }
        self.updateDBDetailList();
        self.dbs.getList(function(dbList) {
          $(self.el).html(self.template.render({
            dbs: _.pluck(dbList, "name"),
            byAddress: byAddress,
            type: self.type
          }));
          $(self.el).append(self.modal.render({}));
          self.replaceSVGs();
          /* this.loadHistory(); */
          self.getServerStatistics(function() {
            self.data = self.generatePieData();
            self.renderPieChart(self.data);
            self.renderLineChart();
            self.updateDBDetailList();
            self.startUpdating();
          });
        });
      });
    },

    generatePieData: function() {
      var pieData = [];
      var self = this;

      this.data.forEach(function(m) {
        pieData.push({key: m.get("name"), value: m.get("system").virtualSize,
          time: self.serverTime});
      });

      return pieData;
    },

    /*
     loadHistory : function() {
       this.hist = {};

       var self = this;
       var coord = this.coordinators.findWhere({
         status: "ok"
       });

       var endpoint = coord.get("protocol")
       + "://"
       + coord.get("address");

       this.dbservers.forEach(function (dbserver) {
         if (dbserver.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(dbserver.id) === -1) {
           self.knownServers.push(dbserver.id);
         }

         var server = {
           raw: dbserver.get("address"),
           isDBServer: true,
           target: encodeURIComponent(dbserver.get("name")),
           endpoint: endpoint,
           addAuth: window.App.addAuth.bind(window.App)
         };
       });

       this.coordinators.forEach(function (coordinator) {
         if (coordinator.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(coordinator.id) === -1) {
           self.knownServers.push(coordinator.id);
         }

         var server = {
           raw: coordinator.get("address"),
           isDBServer: false,
           target: encodeURIComponent(coordinator.get("name")),
           endpoint: coordinator.get("protocol") + "://" + coordinator.get("address"),
           addAuth: window.App.addAuth.bind(window.App)
         };
       });
     },
     */

    addStatisticsItem: function(name, time, requests, snap) {
      var self = this;

      if (! self.hasOwnProperty('hist')) {
        self.hist = {};
      }

      if (! self.hist.hasOwnProperty(name)) {
        self.hist[name] = [];
      }

      var h = self.hist[name];
      var l = h.length;

      if (0 === l) {
        h.push({
          time: time,
          snap: snap,
          requests: requests,
          requestsPerSecond: 0
        });
      }
      else {
        var lt = h[l - 1].time;
        var tt = h[l - 1].requests;

        if (tt < requests) {
          var dt = time - lt;
          var ps = 0;

          if (dt > 0) {
            ps = (requests - tt) / dt;
          }

          h.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: ps
          });
        }
        /*
        else {
          h.times.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: 0
          });
        }
        */
      }
    },

    getServerStatistics: function(nextStep) {
      var self = this;
      var snap = Math.round(self.serverTime / 1000);

      this.data = undefined;

      var statCollect = new window.ClusterStatisticsCollection();
      var coord = this.coordinators.first();

      // create statistics collector for DB servers
      this.dbservers.forEach(function (dbserver) {
        if (dbserver.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(dbserver.id) === -1) {
          self.knownServers.push(dbserver.id);
        }

        var stat = new window.Statistics({name: dbserver.id});

        stat.url = coord.get("protocol") + "://"
        + coord.get("address")
        + "/_admin/clusterStatistics?DBserver="
        + dbserver.get("name");

        statCollect.add(stat);
      });

      // create statistics collector for coordinator
      this.coordinators.forEach(function (coordinator) {
        if (coordinator.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(coordinator.id) === -1) {
          self.knownServers.push(coordinator.id);
        }

        var stat = new window.Statistics({name: coordinator.id});

        stat.url = coordinator.get("protocol") + "://"
        + coordinator.get("address")
        + "/_admin/statistics";

        statCollect.add(stat);
      });

      var cbCounter = statCollect.size();

      this.data = [];

      var successCB = function(m) {
        cbCounter--;
        var time = m.get("time");
        var name = m.get("name");
        var requests = m.get("http").requestsTotal;

        self.addStatisticsItem(name, time, requests, snap);
        self.data.push(m);
        if (cbCounter === 0) {
          nextStep();
        }
      };
      var errCB = function() {
        cbCounter--;
        if (cbCounter === 0) {
          nextStep();
        }
      };
      // now fetch the statistics
      statCollect.fetch(successCB, errCB);
    },

    renderPieChart: function(dataset) {
      var w = $("#clusterGraphs svg").width();
      var h = $("#clusterGraphs svg").height();
      var radius = Math.min(w, h) / 2; //change 2 to 1.4. It's hilarious.
      // var color = d3.scale.category20();
      var color = this.dygraphConfig.colors;

      var arc = d3.svg.arc() //each datapoint will create one later.
      .outerRadius(radius - 20)
      .innerRadius(0);
      var pie = d3.layout.pie()
      .sort(function (d) {
        return d.value;
      })
      .value(function (d) {
        return d.value;
      });
      d3.select("#clusterGraphs").select("svg").remove();
      var pieChartSvg = d3.select("#clusterGraphs").append("svg")
      // .attr("width", w)
      // .attr("height", h)
      .attr("class", "clusterChart")
      .append("g") //someone to transform. Groups data.
      .attr("transform", "translate(" + w / 2 + "," + ((h / 2) - 10) + ")");

    var arc2 = d3.svg.arc()
    .outerRadius(radius-2)
    .innerRadius(radius-2);
    var slices = pieChartSvg.selectAll(".arc")
    .data(pie(dataset))
    .enter().append("g")
    .attr("class", "slice");
        slices.append("path")
    .attr("d", arc)
    .style("fill", function (item, i) {
      return color[i % color.length];
    })
    .style("stroke", function (item, i) {
      return color[i % color.length];
    });
        slices.append("text")
    .attr("transform", function(d) { return "translate(" + arc.centroid(d) + ")"; })
    // .attr("dy", "0.35em")
    .style("text-anchor", "middle")
    .text(function(d) {
      var v = d.data.value / 1024 / 1024 / 1024;
      return v.toFixed(2); });

    slices.append("text")
    .attr("transform", function(d) { return "translate(" + arc2.centroid(d) + ")"; })
    // .attr("dy", "1em")
    .style("text-anchor", "middle")
    .text(function(d) { return d.data.key; });
  },

  renderLineChart: function() {
    var self = this;

    var interval = 60 * 20;
    var data = [];
    var hash = [];
    var t = Math.round(new Date().getTime() / 1000) - interval;
    var ks = self.knownServers;
    var f = function() {
      return null;
    };

    var d, h, i, j, tt, snap;

    for (i = 0;  i < ks.length;  ++i) {
      h = self.hist[ks[i]];

      if (h) {
        for (j = 0;  j < h.length;  ++j) {
          snap = h[j].snap;

          if (snap < t) {
            continue;
          }

          if (! hash.hasOwnProperty(snap)) {
            tt = new Date(snap * 1000);

            d = hash[snap] = [ tt ].concat(ks.map(f));
          }
          else {
            d = hash[snap];
          }

          d[i + 1] = h[j].requestsPerSecond;
        }
      }
    }

    data = [];

    Object.keys(hash).sort().forEach(function (m) {
      data.push(hash[m]);
    });

    var options = this.dygraphConfig.getDefaultConfig('clusterRequestsPerSecond');
    options.labelsDiv = $("#lineGraphLegend")[0];
    options.labels = [ "datetime" ].concat(ks);

    self.graph = new Dygraph(
      document.getElementById('lineGraph'),
      data,
      options
    );
  },

  stopUpdating: function () {
    window.clearTimeout(this.timer);
    delete this.graph;
    this.isUpdating = false;
  },

  startUpdating: function () {
    if (this.isUpdating) {
      return;
    }

    this.isUpdating = true;

    var self = this;

    this.timer = window.setInterval(function() {
      self.rerender();
    }, this.interval);
  },


  dashboard: function(e) {
    this.stopUpdating();

    var tar = $(e.currentTarget);
    var serv = {};
    var cur;
    var coord;

    var ip_port = tar.attr("id");
    ip_port = ip_port.replace(/\-/g,'.');
    ip_port = ip_port.replace(/\_/g,':');
    ip_port = ip_port.substr(2);

    serv.raw = ip_port;
    serv.isDBServer = tar.hasClass("dbserver");

    if (serv.isDBServer) {
      cur = this.dbservers.findWhere({
        address: serv.raw
      });
      coord = this.coordinators.findWhere({
        status: "ok"
      });
      serv.endpoint = coord.get("protocol")
      + "://"
      + coord.get("address");
    }
    else {
      cur = this.coordinators.findWhere({
        address: serv.raw
      });
      serv.endpoint = cur.get("protocol")
      + "://"
      + cur.get("address");
    }

    serv.target = encodeURIComponent(cur.get("name"));
    window.App.serverToShow = serv;
    window.App.dashboard();
  },

  getCurrentSize: function (div) {
    if (div.substr(0,1) !== "#") {
      div = "#" + div;
    }
    var height, width;
    $(div).attr("style", "");
    height = $(div).height();
    width = $(div).width();
    return {
      height: height,
      width: width
    };
  },

  resize: function () {
    var dimensions;
    if (this.graph) {
      dimensions = this.getCurrentSize(this.graph.maindiv_.id);
      this.graph.resize(dimensions.width, dimensions.height);
    }
  }
});
}());

/*global window, $, Backbone, templateEngine, _, alert */

(function() {
  "use strict";

  window.ShowShardsView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("showShards.ejs"),

    events: {
      "change #selectDB" : "updateCollections",
      "change #selectCol" : "updateShards"
    },

    initialize: function() {
      this.dbservers = new window.ClusterServers([], {
        interval: 10000
      });
      this.dbservers.fetch({
        async : false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: 10000
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards();
    },

    updateCollections: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      $("#selectCol").html("");
      _.each(_.pluck(this.cols.getList(dbName), "name"), function(c) {
        $("#selectCol").append("<option id=\"" + c + "\">" + c + "</option>");
      });
      this.updateShards();
    },

    updateShards: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");
      var list = this.shards.getList(dbName, colName);
      $(".shardContainer").empty();
      _.each(list, function(s) {
        var item = $("#" + s.server + "Shards");
        $(".collectionName", item).html(s.server + ": " + s.shards.length);
        /* Will be needed in future
        _.each(s.shards, function(shard) {
          var shardIcon = document.createElement("span");
          shardIcon = $(shardIcon);
          shardIcon.toggleClass("fa");
          shardIcon.toggleClass("fa-th");
          item.append(shardIcon);
        });
        */
      });
    },

    render: function() {
      $(this.el).html(this.template.render({
        names: this.dbservers.pluck("name"),
        dbs: _.pluck(this.dbs.getList(), "name")
      }));
      this.updateCollections();
    }
  });

}());

/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";
  window.ShutdownButtonView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "click #clusterShutdown"  : "clusterShutdown"
    },

    initialize: function() {
      this.overview = this.options.overview;
    },

    template: templateEngine.createTemplate("shutdownButtonView.ejs"),

    clusterShutdown: function() {
      this.overview.stopUpdating();
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is shutting down');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/shutdown",
        success: function(data) {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("handleClusterDown", {trigger: true});
        }
      });
    },

    render: function () {
      $(this.el).html(this.template.render({}));
      return this;
    },

    unrender: function() {
      $(this.el).html("");
    }
  });
}());


/*global window, $, Backbone, document, arangoCollectionModel,arangoHelper,
arangoDatabase, btoa, _*/

(function() {
  "use strict";

  window.ClusterRouter = Backbone.Router.extend({

    routes: {
      ""                       : "initialRoute",
      "planScenario"           : "planScenario",
      "planTest"               : "planTest",
      "planAsymmetrical"       : "planAsymmetric",
      "shards"                 : "showShards",
      "showCluster"            : "showCluster",
      "handleClusterDown"      : "handleClusterDown"
    },

    // Quick fix for server authentication
    addAuth: function (xhr) {
      var u = this.clusterPlan.get("user");
      if (!u) {
        xhr.abort();
        if (!this.isCheckingUser) {
          this.requestAuth();
        }
        return;
      }
      var user = u.name;
      var pass = u.passwd;
      var token = user.concat(":", pass);
      xhr.setRequestHeader('Authorization', "Basic " + btoa(token));
    },

    requestAuth: function() {
      this.isCheckingUser = true;
      this.clusterPlan.set({"user": null});
      var modalLogin = new window.LoginModalView();
      modalLogin.render();
    },

    getNewRoute: function(last) {
      if (last === "statistics") {
        return this.clusterPlan.getCoordinator()
          + "/_admin/"
          + last;
      }
      return this.clusterPlan.getCoordinator()
        + "/_admin/aardvark/cluster/"
        + last;
    },

    initialRoute: function() {
      this.initial();
    },

    updateAllUrls: function() {
      _.each(this.toUpdate, function(u) {
        u.updateUrl();
      });
    },

    registerForUpdate: function(o) {
      this.toUpdate.push(o);
      o.updateUrl();
    },

    initialize: function () {
      this.footerView = new window.FooterView();
      this.footerView.render();

      var self = this;
      this.dygraphConfig = window.dygraphConfig;
      window.modalView = new window.ModalView();
      this.initial = this.planScenario;
      this.isCheckingUser = false;
      this.bind('all', function(trigger, args) {
        var routeData = trigger.split(":");
        if (trigger === "route") {
          if (args !== "showCluster") {
            if (self.showClusterView) {
              self.showClusterView.stopUpdating();
              self.shutdownView.unrender();
            }
            if (self.dashboardView) {
              self.dashboardView.stopUpdating();
            }
          }
        }
      });
      this.toUpdate = [];
      this.clusterPlan = new window.ClusterPlan();
      this.clusterPlan.fetch({
        async: false
      });
      $(window).resize(function() {
        self.handleResize();
      });
    },

    showCluster: function() {
      if (!this.showClusterView) {
        this.showClusterView = new window.ShowClusterView(
            {dygraphConfig : this.dygraphConfig}
      );
      }
      if (!this.shutdownView) {
        this.shutdownView = new window.ShutdownButtonView({
          overview: this.showClusterView
        });
      }
      this.shutdownView.render();
      this.showClusterView.render();
    },

    showShards: function() {
      if (!this.showShardsView) {
        this.showShardsView = new window.ShowShardsView();
      }
      this.showShardsView.render();
    },

    handleResize: function() {
        if (this.dashboardView) {
            this.dashboardView.resize();
        }
        if (this.showClusterView) {
            this.showClusterView.resize();
        }
    },

    planTest: function() {
      if (!this.planTestView) {
        this.planTestView = new window.PlanTestView(
          {model : this.clusterPlan}
        );
      }
      this.planTestView.render();
    },

    planAsymmetric: function() {
      if (!this.planSymmetricView) {
        this.planSymmetricView = new window.PlanSymmetricView(
          {model : this.clusterPlan}
        );
      }
      this.planSymmetricView.render(false);
    },

    planScenario: function() {
      if (!this.planScenarioSelector) {
        this.planScenarioSelector = new window.PlanScenarioSelectorView();
      }
      this.planScenarioSelector.render();
    },

    handleClusterDown : function() {
      if (!this.clusterDownView) {
        this.clusterDownView = new window.ClusterDownView();
      }
      this.clusterDownView.render();
    },

    dashboard: function() {
      var server = this.serverToShow;
      if (!server) {
        this.navigate("", {trigger: true});
        return;
      }

      server.addAuth = this.addAuth.bind(this);
      this.dashboardView = new window.ServerDashboardView({
          dygraphConfig: this.dygraphConfig,
          serverToShow : this.serverToShow
      });
      this.dashboardView.render();
    },

    clusterUnreachable: function() {
      if (this.showClusterView) {
        this.showClusterView.stopUpdating();
        this.shutdownView.unrender();
      }
      if (!this.unreachableView) {
        this.unreachableView = new window.ClusterUnreachableView();
      }
      this.unreachableView.render();
    }

  });

}());

/*global window, $, Backbone, document */

(function() {
  "use strict";

  $.get("cluster/amIDispatcher", function(data) {
    if (!data) {
      var url = window.location.origin;
      url += window.location.pathname;
      url = url.replace("cluster", "index");
      window.location.replace(url);
    }
  });
  window.location.hash = "";
  $(document).ready(function() {
    window.App = new window.ClusterRouter();

    Backbone.history.start();

    if(window.App.clusterPlan.get("plan")) {
      if(window.App.clusterPlan.isAlive()) {
        window.App.initial = window.App.showCluster;
      } else {
        window.App.initial = window.App.handleClusterDown;
      }
    } else {
      window.App.initial = window.App.planScenario;
    }
    window.App.initialRoute();
    window.App.handleResize();
  });

}());
