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
        var id = serv.replace(":", "\\:"),
          type;
        type = $("#" + id).attr("class").split(/\s+/)[1];
        $("#" + id).attr("class", "dbserver " + type + " " + stat);
      });
      this.coordinators.getStatuses(function(stat, serv) {
        var id = serv.replace(":", "\\:"),
          type;
        type = $("#" + id).attr("class").split(/\s+/)[1];
        $("#" + id).attr("class", "coordinator " + type + " " + stat);
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
      this.replaceSVGs();
      this.loadHistory();
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
            if (self.knownServers.indexOf(dbserver.id) === -1) {self.knownServers.push(dbserver.id);}
            var server = {
              raw: dbserver.get("address"),
              isDBServer: true,
              target: encodeURIComponent(dbserver.get("name")),
              endpoint: endpoint
            };
            self.documentStore.getStatisticsHistory({server: server, figures : ["client.totalTime"]});
            self.history = self.documentStore.history;
            self.hist[dbserver.id] = {};
            self.history.forEach(function(e) {
                var date = new Date(e.time * 1000);
                date.setMilliseconds(0);
                date.setSeconds(Math.round(date.getSeconds() / 10) * 10);
                var uptime = e.server.uptime * 1000
                var time = date.getTime();
                if (self.hist[dbserver.id].lastTime && (time - self.hist[dbserver.id].lastTime) > uptime) {
                    self.hist[dbserver.id][
                        self.hist[dbserver.id].lastTime +
                            (time-self.hist[dbserver.id].lastTime) / 2] = null;
                }
                self.hist[dbserver.id].lastTime = time;
                self.hist[dbserver.id][time] = e.client.totalTime.sum / e.client.totalTime.count;
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
            console.log(server, coordinator.id);
            self.documentStore.getStatisticsHistory({server: server, figures : ["client.totalTime"]});
            console.log(self.documentStore.history);
            self.history = self.documentStore.history;
            self.hist[coordinator.id] = {};
            self.history.forEach(function(e) {
                var date = new Date(e.time * 1000);
                date.setMilliseconds(0);
                date.setSeconds(Math.round(date.getSeconds() / 10) * 10);
                var uptime = e.server.uptime * 1000
                var time = date.getTime();
                if (self.hist[coordinator.id].lastTime && (time - self.hist[coordinator.id].lastTime) > uptime) {
                    self.hist[coordinator.id][
                        self.hist[coordinator.id].lastTime +
                            (time-self.hist[coordinator.id].lastTime) / 2] = null;
                }
                self.hist[coordinator.id].lastTime = time;
                self.hist[coordinator.id][time] = e.client.totalTime.sum / e.client.totalTime.count;
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
            var uptime = m.get("server").uptime * 1000
            var time = self.serverTime;
            if (self.hist[m.get("name")].lastTime && (time - self.hist[m.get("name")].lastTime) > uptime) {
                self.hist[m.get("name")][
                    self.hist[m.get("name")].lastTime +
                        (time-self.hist[m.get("name")].lastTime) / 2] = null;
            }
            self.hist[m.get("name")].lastTime = time;
            self.hist[m.get("name")][time] = m.get("client").totalTime.sum / m.get("client").totalTime.count;
        });
        this.data = statCollect;
    },

    renderPieChart: function(dataset) {
        var w = 280;
        var h = 160;
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
          self.chartData = {
              labelsNormal : ['datetime'],
              labelsShowAll : ['datetime'],
              data : [],
              visibilityNormal : [],
              visibilityShowAll : []
          };
          var getData = function() {
              var data = {};
              Object.keys(self.hist).forEach(function(server) {
                  Object.keys(self.hist[server]).forEach(function(date) {
                    if (date === "lastTime") {
                        return;
                    }
                    if (!data[date]) {
                        data[date] = {}
                        Object.keys(self.hist).forEach(function(s) {
                            data[date][s] = null;
                        });
                    }
                    data[date][server] = self.hist[server][date];
                  });
              });
              Object.keys(data).forEach(function(d) {
                  var i = 0;
                  var sum = 0;
                  Object.keys(data[d]).forEach(function(server) {
                    if (data[d][server] !== null) {
                        i++;
                        sum = sum + data[d][server];
                    }
                    data[d]["ClusterAverage"] = sum/i;
                  });
              });
              Object.keys(data).sort().forEach(function (time) {
                  var dataList = [new Date(parseFloat(time))];
                  self.max = Number.NEGATIVE_INFINITY;
                  self.chartData.visibilityShowAll = [];
                  self.chartData.labelsShowAll = [ "Date"];
                  Object.keys(data[time]).sort().forEach(function (server) {
                      self.chartData.visibilityShowAll.push(true);
                      self.chartData.labelsShowAll.push(server);
                      dataList.push(data[time][server]);
                  });
                  self.chartData.data.push(dataList);
              });
              var latestEntry = self.chartData.data[self.chartData.data.length -1];
              latestEntry.forEach(function (e) {
                  if (latestEntry.indexOf(e) > 0) {
                      if (e !== null) {
                          if (self.max < e) {
                              self.max = e;
                          }
                      }
                  }
              });
              self.chartData.visibilityNormal = [];
              self.chartData.labelsNormal = [ "Date"];
              var i = 0;
              latestEntry.forEach(function (e) {
                  if (i > 0) {
                      if ("ClusterAverage" === self.chartData.labelsShowAll[i]) {
                          self.chartData.visibilityNormal.push(true);
                          self.chartData.labelsNormal.push(self.chartData.labelsShowAll[i] + " (avg)");
                      } else if (e === self.max ) {
                          self.chartData.visibilityNormal.push(true);
                          self.chartData.labelsNormal.push(self.chartData.labelsShowAll[i] + " (max)");
                      } else {
                          self.chartData.visibilityNormal.push(false);
                          self.chartData.labelsNormal.push(self.chartData.labelsShowAll[i]);
                      }
                  }
                  i++
              });
          };

          if (this.graph !== undefined) {
              getData();
              var opts = {file : this.chartData.data};
              if (this.graphShowAll ) {
                opts.labels = this.chartData.labelsShowAll;
                opts.visibility = this.chartData.visibilityShowAll;
              } else {
                opts.labels = this.chartData.labelsNormal;
                opts.visibility = this.chartData.visibilityNormal;
              }
              this.graph.updateOptions(opts);
              return;
          }

          var makeGraph = function(className) {
            getData(),
                console.log(self.chartData);
            self.graph = new Dygraph(
                  document.getElementById('lineGraph'),
                  self.chartData.data,
                  {   title: 'Average request time in milliseconds',
                      labelsDivStyles: { 'backgroundColor': '#e1e3e5','textAlign': 'right' },
                      //labelsSeparateLines: true,
                      connectSeparatedPoints : false,
                      digitsAfterDecimal: 3,
                      fillGraph : true,
                      strokeWidth: 2,
                      legend: "always",
                      axisLabelFont: "Open Sans",
                      //dateWindow : [new Date().getTime() - 20 * 60 * 1000,new Date().getTime()],
                      labels: self.chartData.labelsNormal,
                      visibility: self.chartData.visibilityNormal ,
                      //xAxisLabelWidth : "60",
                      showRangeSelector: false,
                      rightGap: 15,
                      pixelsPerLabel : 60,
                      //labelsKMG2: false,
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
        window.App.dashboard();
      }
    });

  }());
