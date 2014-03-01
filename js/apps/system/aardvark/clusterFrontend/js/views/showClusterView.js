/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, templateEngine, alert */

(function() {
  "use strict";

  window.ShowClusterView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("showCluster.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    modalDummy: templateEngine.createTemplate("modalDashboardDummy.ejs"),

      events: {
        "change #selectDB"        : "updateCollections",
        "change #selectCol"       : "updateShards",
        "click .dbserver"         : "dashboard",
        "mouseover #lineGraph"    : "setShowAll",
        "mouseout #lineGraph"     : "resetShowAll"
      },

      updateServerTime: function() {
          this.serverTime = new Date().getTime();
      },

      setShowAll: function() {
          this.graphShowAll = true;
          this.renderLineChart();
      },

      resetShowAll: function() {
          this.graphShowAll = false;
          this.renderLineChart();
      },


      initialize: function() {
        this.interval = 10000;
        this.isUpdating = false;
        this.timer = null;
        this.totalTimeChart = {};
        this.knownServers = [];
        this.graph = undefined;
        this.graphShowAll = false;
        this.updateServerTime();


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
        $("#" + s.server + "Shards").html(s.shards.length)
      });
    },

    updateServerStatus: function() {
      this.dbservers.getStatuses(function(stat, serv) {
        $("#" + serv.replace(":", "\\:")).attr("class", "dbserver " + stat);
      });
      this.coordinators.getStatuses(function(stat, serv) {
        $("#" + serv.replace(":", "\\:")).attr("class", "coordinator " + stat);
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
      } else {
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
      this.transformForLineChart();
      this.renderLineChart();
      this.updateDBDetailList();
    },

    render: function() {
      this.startUpdating();
      var byAddress = this.listByAddress();
      if (Object.keys(byAddress).length === 1) {
        this.type = "testPlan";
      } else {
        this.type = "other";
      }
      $(this.el).html(this.template.render({
        dbs: _.pluck(this.dbs.getList(), "name"),
        byAddress: byAddress,
        type: this.type
      }));
      $(this.el).append(this.modal.render({}));
      this.loadHistory();
      this.getServerStatistics();
      this.data = this.generatePieData();
      this.renderPieChart(this.data);
      this.transformForLineChart();
      this.renderLineChart();
      this.updateCollections();
    },

    generatePieData: function() {
        var pieData = [];
        var self = this;
        this.data.forEach(function(m) {
            pieData.push({key: m.get("name"), value :m.get("system").virtualSize,
                    time: self.serverTime});
        });
        return pieData;
    },

    transformForLineChart: function() {
        var c = 0;
        var self = this;
        var data = this.hist;
        data.forEach(function(entry) {
            c++;
            if (!self.totalTimeChart[entry.time]) {
                self.totalTimeChart[entry.time] = {};
            }
            if (!self.totalTimeChart[entry.time]["ClusterAverage"]) {
                self.totalTimeChart[entry.time]["ClusterAverage"] = 0;
            }
            self.totalTimeChart[entry.time][entry.key] = entry.value;
            self.totalTimeChart[entry.time]["ClusterAverage"] =
                self.totalTimeChart[entry.time]["ClusterAverage"] + entry.value;
        })
        self.knownServers.forEach(function (server) {
            Object.keys(self.totalTimeChart).sort().forEach(function(entry) {
                if (!self.totalTimeChart[entry][server]) {
                    self.totalTimeChart[entry][server] = null;
                }
            })
        })
    },

    loadHistory : function() {
        this.hist = [];
        var self = this;
        var coord = this.coordinators.findWhere({
          status: "ok"
        });
        var endpoint = coord.get("protocol")
          + "://"
          + coord.get("address");
        this.dbservers.forEach(function (dbserver) {
            if (dbserver.get("status") !== "ok") {return;}
            if (self.knownServers.indexOf(dbserver.id) === -1) {self.knownServers.push(dbserver.id);}
            var server = {
              raw: dbserver.get("address"),
              isDBServer: true,
              target: encodeURIComponent(dbserver.get("name")),
              endpoint: endpoint
            };
            self.documentStore.getStatisticsHistory({server: server, figures : ["client.totalTime"]});
            self.history = self.documentStore.history;
            self.history.forEach(function(e) {
                var h = {};
                h.key = dbserver.id;
                h.value = e.client.totalTime.sum / e.client.totalTime.count;
                h.time = e.time * 1000
                self.hist.push(h);
            });
        });
        this.coordinators.forEach(function (coordinator) {
            if (coordinator.get("status") !== "ok") {return;}
            if (self.knownServers.indexOf(coordinator.id) === -1) {self.knownServers.push(coordinator.id);}
            var server = {
              raw: coordinator.get("address"),
              isDBServer: false,
              target: encodeURIComponent(coordinator.get("name")),
              endpoint: coordinator.get("protocol") + "://" + coordinator.get("address")
            };
            self.documentStore.getStatisticsHistory({server: server, figures : ["client.totalTime"]});
            self.history = self.documentStore.history;
            self.history.forEach(function(e) {
                var h = {};
                h.key = coordinator.id;
                h.value = e.client.totalTime.sum / e.client.totalTime.count;
                h.time = e.time * 1000
                self.hist.push(h);
            });
        });
    },

    getServerStatistics: function() {
        var self = this;
        this.data = undefined;
        var statCollect = new window.ClusterStatisticsCollection();
        this.dbservers.forEach(function (dbserver) {
            if (dbserver.get("status") !== "ok") {return;}
            if (self.knownServers.indexOf(dbserver.id) === -1) {self.knownServers.push(dbserver.id);}
            var stat = new window.Statistics({name: dbserver.id});
            stat.url = "http://" + dbserver.get("address") + "/_admin/statistics";
            statCollect.add(stat);
        });
        this.coordinators.forEach(function (coordinator) {
            if (coordinator.get("status") !== "ok") {return;}
            if (self.knownServers.indexOf(coordinator.id) === -1) {self.knownServers.push(coordinator.id);}
            var stat = new window.Statistics({name: coordinator.id});
            stat.url = "http://" + coordinator.get("address") + "/_admin/statistics";
            statCollect.add(stat);
        });
        statCollect.fetch();
        statCollect.forEach(function(m) {
            self.hist.push({key: m.get("name"), value :m.get("client").totalTime.sum / m.get("client").totalTime.count,
                time: self.serverTime});
        });
        this.data = statCollect;
    },

    renderPieChart: function(dataset) {
        var w = 500;
        var h = 250;
        var radius = Math.min(w, h) / 2; //change 2 to 1.4. It's hilarious.
        var color = d3.scale.category20();

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
            .attr("width", w)
            .attr("height", h)
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

        slices.append("path")
            .attr("d", arc)
            .style("fill", function (d, i) {
                return color(i);
            });
        slices.append("text")
            .attr("transform", function(d) { return "translate(" + arc.centroid(d) + ")"; })
            .attr("dy", ".35em")
            .style("text-anchor", "middle")
            .text(function(d) {
                var v = d.data.value / 1000000000
                return v.toFixed(2) + "GB"; });

        slices.append("text")
            .attr("transform", function(d) { return "translate(" + arc2.centroid(d) + ")"; })
            .attr("dy", ".35em")
            .style("text-anchor", "middle")
            .text(function(d) { return d.data.key; });
      },

      renderLineChart: function() {
          var self = this;

          var getData = function() {
              var data = [];
              self.max = Number.NEGATIVE_INFINITY;
              self.min = Number.POSITIVE_INFINITY;
              Object.keys(self.totalTimeChart).sort().forEach(function(time) {
                  var entry = [new Date(parseInt(time))];
                  Object.keys(self.totalTimeChart[time]).sort().forEach(function(server) {
                      if (self.min > self.totalTimeChart[time][server]) {
                          self.min = self.totalTimeChart[time][server];
                      }
                      if (self.max < self.totalTimeChart[time][server]) {
                          self.max = self.totalTimeChart[time][server];
                      }
                      entry.push(self.totalTimeChart[time][server]);
                  })
                  data.push(entry);
              });
              return data;
          };
          var getVisibility = function() {
              var setFalse = function(list, index, skip) {
                  for (var i = 0; i < index; i ++ ) {
                      if (i !== skip) {
                        list[i] = self.graphShowAll;
                      }
                  }
              }
              var latestTime = Object.keys(self.totalTimeChart).sort().reverse()[0];
              var visibility = [], max= 0, i = 0, skip = -1;
              Object.keys(self.totalTimeChart[latestTime]).sort().forEach(function(server) {
                  i++;
                  if (server === "ClusterAverage") {
                      skip = i-1;
                      visibility.push(true);
                  } else if (max < self.totalTimeChart[latestTime][server]) {
                      max = self.totalTimeChart[latestTime][server];
                      setFalse(visibility, i-1, skip);
                      visibility.push(true);
                  } else {
                      visibility.push(self.graphShowAll);
                  }
                });
              return visibility;
          };
          var createLabels = function() {
              var labels = ['datetime'];
              Object.keys(self.totalTimeChart[Object.keys(self.totalTimeChart)[0]]).sort().forEach(function(server) {
                  if (!self.graphShowAll) {
                    if (server === "ClusterAverage") {
                        labels.push(server + "(avg)");
                    }  else {
                        labels.push(server + "(max)");
                    }
                  } else {
                    labels.push(server);
                  }
              });
              return labels.slice();
          }



          if (this.graph !== undefined) {
              this.graph.updateOptions( {
                  'file': getData(),
                  'labels': createLabels(),
                  'visibility' : getVisibility(),
                  'valueRange': [self.min -0.1 * self.min, self.max + 0.1 * self.max]
              } );
              return;
          }

          var makeGraph = function(className) {

            self.graph = new Dygraph(
                  document.getElementById('lineGraph'),
                  getData(),
                  {   title: 'Average request time in milliseconds',
                      labelsDivStyles: { 'backgroundColor': '#e1e3e5','textAlign': 'right' },
                      labelsSeparateLines: true,
                      connectSeparatedPoints : true,
                      digitsAfterDecimal: 3,
                      fillGraph : true,
                      strokeWidth: 2,
                      legend: "always",
                      axisLabelFont: "Open Sans",
                      dateWindow : [new Date().getTime() - 20 * 60 * 1000,new Date().getTime()],
                      //labels: createLabels(),
                      //visibility:getVisibility() ,
                      xAxisLabelWidth : "60",
                      showRangeSelector: false,
                      rightGap: 15,
                      pixelsPerLabel : 60,
                      labelsKMG2: false,
                      highlightCircleSize: 2
                      });
              var onclick = function(ev) {
                  if (self.graph.isSeriesLocked()) {
                      self.graph.clearSelection();
                  } else {
                      self.graph.setSelection(self.graph.getSelection(), self.graph.getHighlightSeries(), true);
                  }
              };
              self.graph.updateOptions({clickCallback: onclick}, true);
              self.graph.setSelection(false, 'ClusterAverage', true);
          };
          makeGraph('lineGraph');

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
        $("#modalPlaceholder").html(this.modalDummy.render({}));
        serv.raw = tar.attr("id");
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
        } else {
          cur = this.coordinators.findWhere({
            address: serv.raw
          });
          serv.endpoint = cur.get("protocol")
            + "://"
            + cur.get("address");
        }
        serv.target = encodeURIComponent(cur.get("name"));
        window.App.serverToShow = serv;
        window.App.navigate("dashboard", {trigger: true});
    }
  });

}());
