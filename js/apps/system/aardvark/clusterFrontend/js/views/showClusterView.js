/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ShowClusterView = Backbone.View.extend({

    el: "#content",

    template: clusterTemplateEngine.createTemplate("showCluster.ejs"),

    initialize: function() {
        this.interval = 3000;
        this.isUpdating = false;
        this.timer = null;
        this.totalTimeChart = {};
        this.knownServers = [];

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
      this.render(this.server);
    },

    render: function(server) {
      this.server = server;
      var self = this;
      var statCollect = new window.ClusterStatisticsCollection();
      this.dbservers.forEach(function (dbserver) {
        if (self.knownServers.indexOf(dbserver.id) === -1) {self.knownServers.push(dbserver.id);}
        var stat = new window.Statistics({name: dbserver.id});
        stat.url = dbserver.get("address").replace("tcp", "http") + "/_admin/statistics";
        statCollect.add(stat);
      });
      this.coordinators.forEach(function (coordinator) {
        if (self.knownServers.indexOf(coordinator.id) === -1) {self.knownServers.push(coordinator.id);}
        var stat = new window.Statistics({name: coordinator.id});
        stat.url = coordinator.get("address").replace("tcp", "http") + "/_admin/statistics";
        statCollect.add(stat);
      });
      statCollect.fetch();
      this.data = statCollect;
      $(this.el).html(this.template.render({}));
      var serverTime = new Date().getTime();
      var pieData = [];
      this.data.forEach(function(m) {
          pieData.push({key: m.get("name"), value :m.get("client").totalTime.sum / m.get("http").requestsTotal,
              time: serverTime});
      });
      self.renderPieChart(pieData);
      var transformForLineChart = function(data) {
          var c = 0;
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
                  (self.totalTimeChart[entry.time]["ClusterAverage"] + entry.value) / c;
          })
          self.knownServers.forEach(function (server) {
              Object.keys(self.totalTimeChart).sort().forEach(function(entry) {
                if (!self.totalTimeChart[entry][server]) {
                    self.totalTimeChart[entry][server] = 0;
                }
              })
          })

      }
      transformForLineChart(pieData);
      self.renderLineChart();
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

        var svg = d3.select("#clusterGraphs").append("svg")
            .attr("width", w)
            .attr("height", h)
            .attr("class", "clusterChart")
            .append("g") //someone to transform. Groups data.
            .attr("transform", "translate(" + w / 2 + "," + h / 2 + ")");

        var arc2 = d3.svg.arc()
            .outerRadius(radius-4)
            .innerRadius(radius-4);

        var slices = svg.selectAll(".arc")
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
              Object.keys(self.totalTimeChart).sort().forEach(function(time) {
                  var entry = [time];
                  Object.keys(self.totalTimeChart[time]).sort().forEach(function(server) {
                      entry.push(self.totalTimeChart[time][server]);
                  })
                  data.push(entry);
              })

              console.log(data);
              return data;
          };
          var makeGraph = function(className) {
              var lineGraph = document.getElementById('lineGraph');
              var div = document.createElement('div');
              div.className = className;
              div.style.display = 'inline-block';
              div.style.margin = '4px';
              lineGraph.appendChild(div);

              var labels = ['x'];
              Object.keys(self.totalTimeChart[Object.keys(self.totalTimeChart)[0]]).sort().forEach(function(server) {
                 labels.push(server);
              })
              var g = new Dygraph(
                  div,
                  getData(),
                  {
                      axes: {
                          x: {
                              axisLabelFormatter: function(d, gran) {
                                  var date = new Date(d);
                                  return Dygraph.zeropad(date.getHours()) + ":"
                                      + Dygraph.zeropad(date.getMinutes()) + ":"
                                      + Dygraph.zeropad(date.getSeconds());
                              }
                          }
                      },
                      width: 480,
                      height: 320,
                      labels: labels.slice(),
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
                  if (g.isSeriesLocked()) {
                      g.clearSelection();
                  } else {
                      g.setSelection(g.getSelection(), g.getHighlightSeries(), true);
                  }
              };
              g.updateOptions({clickCallback: onclick}, true);
              g.setSelection(false, 's005');
              //console.log(g);
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
