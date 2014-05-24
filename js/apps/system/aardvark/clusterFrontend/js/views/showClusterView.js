/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
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
      "change #selectDB"        : "updateCollections",
      "change #selectCol"       : "updateShards",
      "click .dbserver"         : "dashboard",
      "click .coordinator"      : "dashboard",
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

    listByAddress: function() {
      var byAddress = this.dbservers.byAddress();
      byAddress = this.coordinators.byAddress(byAddress);
      return byAddress;
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
      $(".shardCounter").html("0");
      _.each(list, function(s) {
        $("#" + s.server + "Shards").html(s.shards.length);
      });
    },

    updateServerStatus: function() {
      this.dbservers.getStatuses(function(stat, serv) {
        var id = serv,
          type;
        id = id.replace(/\./g,'-');
        id = id.replace(/\:/g,'_');
        type = $("#id" + id).attr("class").split(/\s+/)[1];
        $("#id" + id).attr("class", "dbserver " + type + " " + stat);
      });
      this.coordinators.getStatuses(function(stat, serv) {
        var id = serv,
          type;
        id = id.replace(/\./g,'-');
        id = id.replace(/\:/g,'_');
        type = $("#id" + id).attr("class").split(/\s+/)[1];
        $("#id" + id).attr("class", "coordinator " + type + " " + stat);
      });
    },

    updateDBDetailList: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");

      var selDB = $("#selectDB");
      selDB.html("");
      _.each(_.pluck(this.dbs.getList(), "name"), function(c) {
        selDB.append("<option id=\"" + c + "\">" + c + "</option>");
      });
      var dbToSel = $("#" + dbName, selDB);
      if (!dbToSel.length) {
        dbName = $("#selectDB").find(":selected").attr("id");
      }
      else {
        dbToSel.prop("selected", true);
      }

      var selCol = $("#selectCol");
      selCol.html("");
      _.each(_.pluck(this.cols.getList(dbName), "name"), function(c) {
        selCol.append("<option id=\"" + c + "\">" + c + "</option>");
      });
      var colToSel = $("#" + colName, selCol);
      colToSel.prop("selected", true);
      if (!colToSel.length || !dbToSel.length) {
        this.updateShards();
      }
    },

    rerender : function() {
      this.updateServerStatus();
      this.getServerStatistics();
      this.updateServerTime();
      this.data = this.generatePieData();
      this.renderPieChart(this.data);
      this.renderLineChart();
      this.updateDBDetailList();
    },

    render: function() {
      this.startUpdating();
      var byAddress = this.listByAddress();
      if (Object.keys(byAddress).length === 1) {
        this.type = "testPlan";
      }
      else {
        this.type = "other";
      }
      $(this.el).html(this.template.render({
        dbs: _.pluck(this.dbs.getList(), "name"),
        byAddress: byAddress,
        type: this.type
      }));
      $(this.el).append(this.modal.render({}));
      this.replaceSVGs();
      /* this.loadHistory(); */
      this.getServerStatistics();
      this.data = this.generatePieData();
      this.renderPieChart(this.data);
      this.renderLineChart();
      this.updateCollections();
    },

    generatePieData: function() {
      var pieData = [];
      var self = this;

      this.data.forEach(function(m) {
        pieData.push({key: m.get("name"), value :m.get("system").residentSize,
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

        if (tt >= requests) {
          h.times.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: 0
          });
        }
        else {
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
      }
    },

    getServerStatistics: function() {
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

      // now fetch the statistics
      statCollect.fetch();

      statCollect.forEach(function(m) {
        var time = m.get("time");
        var name = m.get("name");
        var requests = m.get("http").requestsTotal;

        self.addStatisticsItem(name, time, requests, snap);
      });

      this.data = statCollect;
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
            .attr("transform", "translate(" + w / 2 + "," + h / 2 + ")");

        var arc2 = d3.svg.arc()
            .outerRadius(radius-2)
            .innerRadius(radius-2);
        var slices = pieChartSvg.selectAll(".arc")
            .data(pie(dataset))
            .enter().append("g")
            .attr("class", "slice");
        /*jslint unparam: true*/
        slices.append("path")
            .attr("d", arc)
            .style("fill", function (item, i) {
              return color[i % color.length];
            });
        /*jslint unparam: false*/
        slices.append("text")
            .attr("transform", function(d) { return "translate(" + arc.centroid(d) + ")"; })
            .attr("dy", ".35em")
            .style("text-anchor", "middle")
            .text(function(d) {
                var v = d.data.value / 1024 / 1024 / 1024;
                return v.toFixed(2); });

        slices.append("text")
            .attr("transform", function(d) { return "translate(" + arc2.centroid(d) + ")"; })
            .attr("dy", ".35em")
            .style("text-anchor", "middle")
            .text(function(d) { return d.data.key; });
      },

      renderLineChart: function() {
        var self = this;

        var intervall = 60 * 20;
        var data = [];
        var hash = [];
        var t = Math.round(new Date().getTime() / 1000) - intervall;
        var ks = self.knownServers;
        var i;

        var f = function(x) {return null;};

        for (i = 0;  i < intervall;  ++i) {
          var dt = t + i;
          var tt = new Date(dt * 1000);
          var d = [ tt ].concat(ks.map(f));

          data.push(d);
          hash[dt] = d;
        }

        var j;

        for (i = 0;  i < ks.length;  ++i) {
          var h = self.hist[ks[i]];

          if (h) {
            for (j = 0;  j < h.length;  ++j) {
              var snap = h[j].snap;

              if (hash.hasOwnProperty(snap)) {
                var dd = hash[snap];

                dd[i + 1] = h[j].requestsPerSecond;
              }
            }
          }
        }

        var options = this.dygraphConfig.getDefaultConfig('clusterRequestsPerSecond');
        options.labelsDiv = $("#lineGraphLegend")[0];
        
        labels: ["datetime", "Major Page", "Minor Page"],

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
