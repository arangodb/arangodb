/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ShowClusterView = Backbone.View.extend({

    el: "#content",

    template: clusterTemplateEngine.createTemplate("showCluster.ejs"),

      updateServerTime: function() {
          this.serverTime = new Date().getTime();
      },


      initialize: function() {
        this.interval = 1000;
        this.isUpdating = false;
        this.timer = null;
        this.totalTimeChart = {};
        this.knownServers = [];
        this.graph = undefined;
        this.updateServerTime();

        this.dbservers = new window.ClusterServers();
        this.dbservers.startUpdating();
        this.dbservers.fetch({
            async : false
        });
        this.coordinators = new window.ClusterCoordinators();
        this.coordinators.startUpdating();
        this.coordinators.fetch({
            async : false
        });
        if (this.statisticsDescription === undefined) {
            this.statisticsDescription = new window.StatisticsDescription();
            this.statisticsDescription.fetch({
                async:false
            });
        }
        this.dbservers.bind("add", this.rerender.bind(this));
        this.dbservers.bind("change", this.rerender.bind(this));
        this.dbservers.bind("remove", this.rerender.bind(this));
        this.coordinators.bind("add", this.rerender.bind(this));
        this.coordinators.bind("change", this.rerender.bind(this));
        this.coordinators.bind("remove", this.rerender.bind(this));
        this.startUpdating();
    },

    rerender : function() {
        this.getServerStatistics();
        this.updateServerTime();
        var data = this.generatePieData();
        this.renderPieChart(data);
        this.transformForLineChart(data);
        this.renderLineChart();
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      this.getServerStatistics();
      var data = this.generatePieData();
      this.renderPieChart(data);
      this.transformForLineChart(data);
      this.renderLineChart();
    },

    generatePieData: function() {
        var pieData = [];
        var self = this;
        this.data.forEach(function(m) {
            pieData.push({key: m.get("name"), value :m.get("client").totalTime.sum / m.get("http").requestsTotal,
                time: self.serverTime});
        });
        return pieData;
    },

    transformForLineChart: function(data) {
        var c = 0;
        var self = this;
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
        self.totalTimeChart[self.serverTime]["ClusterAverage"] =
            self.totalTimeChart[self.serverTime]["ClusterAverage"] /c;
        self.knownServers.forEach(function (server) {
            Object.keys(self.totalTimeChart).sort().forEach(function(entry) {
                if (!self.totalTimeChart[entry][server]) {
                    self.totalTimeChart[entry][server] = null;
                }
            })
        })
    },

    getServerStatistics: function() {
        var self = this;
        var statCollect = new window.ClusterStatisticsCollection();
        this.dbservers.forEach(function (dbserver) {
            if (dbserver.get("status") !== "ok") {return;}
            if (self.knownServers.indexOf(dbserver.id) === -1) {self.knownServers.push(dbserver.id);}
            var stat = new window.Statistics({name: dbserver.id});
            stat.url = dbserver.get("address").replace("tcp", "http") + "/_admin/statistics";
            statCollect.add(stat);
        });
        this.coordinators.forEach(function (coordinator) {
            if (coordinator.get("status") !== "ok") {return;}
            if (self.knownServers.indexOf(coordinator.id) === -1) {self.knownServers.push(coordinator.id);}
            var stat = new window.Statistics({name: coordinator.id});
            stat.url = coordinator.get("address").replace("tcp", "http") + "/_admin/statistics";
            statCollect.add(stat);
        });
        statCollect.fetch();
        this.data = statCollect;
    },

    renderPieChart: function(dataset) {
        var w = 620;
        var h = 480;
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
            .outerRadius(radius-4)
            .innerRadius(radius-4);

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
            .text(function(d) { return d.data.value.toFixed(2); });

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
              var c = 0
              Object.keys(self.totalTimeChart).sort().forEach(function(time) {
                  var entry = [new Date(parseInt(time))];
                  Object.keys(self.totalTimeChart[time]).sort().forEach(function(server) {
                      entry.push(self.totalTimeChart[time][server]);
                  })
                  data.push(entry);
              })
              return data;
          };
          var createLabels = function() {
              var labels = ['datetime'];
              Object.keys(self.totalTimeChart[Object.keys(self.totalTimeChart)[0]]).sort().forEach(function(server) {
                  labels.push(server);
              })
              return labels.slice();
          }
          if (this.graph !== undefined) {
              this.graph.updateOptions( {
                  'file': getData(),
                  'labels': createLabels()
              } );
              return;
          }



          var makeGraph = function(className) {

            self.graph = new Dygraph(
                  document.getElementById('lineGraph'),
                  getData(),
                  {   title: 'Average request time in the Cluster',
                      ylabel: 'time in milliseconds',
                      labelsDivStyles: { 'textAlign': 'right' },
                      hideOverlayOnMouseOut: true,
                      labelsSeparateLines: true,
                      labelsDivWidth: 150,
                      labelsShowZeroValues: false,
                      highlightSeriesBackgroundAlpha: 0.5,
                      drawPoints: true,
                      width: 480,
                      height: 320,
                      labels: createLabels(),
                      stackedGraph: false,

                      highlightCircleSize: 2,
                      strokeWidth: 1,
                      strokeBorderWidth: null,

                      highlightSeriesOpts: {
                          strokeWidth: 3,
                          strokeBorderWidth: 1,
                          highlightCircleSize: 5
                      }
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
          makeGraph("lineGraph");

      },

      stopUpdating: function () {
          window.clearTimeout(this.timer);
          this.isUpdating = false;
          this.dbservers.stopUpdating();
          this.coordinators.stopUpdating();
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
      }

  });



}());
