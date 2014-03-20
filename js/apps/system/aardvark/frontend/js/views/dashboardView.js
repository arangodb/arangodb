/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.dashboardView = Backbone.View.extend({
    el: '#content',
    contentEl: '.contentDiv',
    distributionChartDiv : "#distributionChartDiv",
    interval: 5000, // in milliseconds
    detailTemplate: templateEngine.createTemplate("lineChartDetailView.ejs"),
    detailEl: '#modalPlaceholder',

    events: {
      "click .innerDashboardChart" : "showDetail",
      "mousedown .dygraph-rangesel-zoomhandle" : "stopUpdating",
      "mouseup .dygraph-rangesel-zoomhandle" : "startUpdating",
      "mouseleave .dygraph-rangesel-zoomhandle" : "startUpdating",
      "click #backToCluster" : "returnToClusterView"
    },
    
    showDetail : function(e) {
      var self = this;
      var id = $(e.currentTarget).attr("id").replace(/LineChart/g , "");
      var group;
      Object.keys(self.series).forEach(function (g) {
        Object.keys(self.series[g]).forEach(function (f) {
          if (f === id) {
            group = g;
          }
        });
      });
      var figures = [];
      if (self.dygraphConfig.combinedCharts[group + "_" + id]) {
        self.dygraphConfig.combinedCharts[group + "_" + id].forEach(function(e) {
          figures.push(group + "." + e);
        });
      } else {
        figures.push(group + "." + id);
      }
      self.getStatisticHistory({
        startDate :  (new Date().getTime() - 7 * 24 * 60 * 60 * 1000) / 1000,
        figures :  figures
      });
      var figs = [{identifier : "uptime", group : "server"}];
      figures.forEach(function(f) {
        figs.push({
          identifier : f.split(".")[1],
          group : f.split(".")[0]
        });
      });
      self.description = {
        get: function(y) {
          if (y === "groups") {
            return [{
              group : group
            }, {
              group : "server"
            }];
          }
          if (y === "figures") {
            return figs;
          }
        }
      };

      self.uptime = undefined;
      var chart = {};
      Object.keys(self.series[group][id]).forEach(function(valueList) {
        var c = self.series[group][id][valueList];
        if (c.type === "current" && c.showGraph === true) {
          chart = c;
          chart.data = [];
        }
      });
      $(self.detailEl).html(this.detailTemplate.render({figure: chart.options.title}));
      self.calculateSeries();
      chart.graphCreated = false;
      self.showDetailFor("dashboardDetailed", id, chart, chart.options.title);
      self.createLineChart(this.detailChart.chart, id,  "dashboardDetailed");
      $('#lineChartDetail').modal('show');
      $('#lineChartDetail').on('hidden', function () {
        self.hidden();
      });
      $('.modalTooltips').tooltip({
        placement: "left"
      });
      return self;
    },

    hidden: function () {
      if (this.modal) {
          $(".modal-backdrop.fade.in").click();
          this.hide();
      }
      delete this.currentChart;
      this.options.description.fetch({
        async:false
      });
      if (this.options.server) {
            this.getStatisticHistory({
                startDate :  (new Date().getTime() - 20 * 60 * 1000) / 1000
            });
            this.calculateSeries();
      }
      $('#' + this.detailChart.figure + "LineChart").empty();
      this.detailChart.chart.graphCreated = false;
      this.detailChart.chart.graph.destroy();
      this.detailChart.chart.defaultConfig();
      this.detailChart.chart.updateLabels();
      this.description = this.options.description.models[0];
      this.detailChart.chart.options.title = this.detailChart.title;
      this.createLineChart(this.detailChart.chart, this.detailChart.figure, this.detailChart.figure, true);
      this.detailChart = {};
     window.App.navigate("#", {trigger: true});
    },

    initialize: function () {
      this.documentStore = this.options.documentStore;
      this.getStatisticHistory({
        startDate :  (new Date().getTime() - 20 * 60 * 1000) / 1000
      });
      if (this.options.server) {
        this.statisticsUrl = this.options.server.endpoint
          + "/_admin/clusterStatistics"
          + "?DBserver=" + this.options.server.target;
      } else {
        this.statisticsUrl = "/_admin/statistics";
      }
      this.description = this.options.description.models[0];
      this.detailChart = {};
      this.dygraphConfig = this.options.dygraphConfig;
      this.startUpdating();
      this.graphs = {};
    },

    prepareSeries: function () {
      var self = this;
      self.series = {};
      self.description.get("groups").forEach(function(group) {
        self.series[group.group] = {};
      });
      self.description.get("figures").forEach(function(figure) {
        self.series[figure.group][figure.identifier] =
            self.dygraphConfig.getChartsForFigure(figure.identifier, figure.type, figure.name);
      });
      Object.keys(self.dygraphConfig.combinedCharts).forEach(function (cc) {
        var part = cc.split("_");
        self.series[part[0]][part[1]] =
            self.dygraphConfig.getChartsForFigure(part[1], self.dygraphConfig.regularLineChartType);
      });
    },


    processSingleStatistic: function (entry) {
      var self = this;
      var time = entry.time * 1000;
      var newUptime = entry.server.uptime;
      if (newUptime != null  && self.uptime && newUptime < self.uptime) {

        var e = {time : (time-(newUptime+0.01)* 1000 ) /1000};
        self.description.get("figures").forEach(function(figure) {
          if (!e[figure.group]) {
            e[figure.group] = {};
          }
          e[figure.group][figure.identifier] = null;
        });
        self.uptime = newUptime;
        self.processSingleStatistic(e);
      }
      self.uptime = newUptime;
      self.description.get("groups").forEach(function(g) {
        Object.keys(entry[g.group]).forEach(function(figure) {
          var valueLists = self.series[g.group][figure];
          Object.keys(valueLists).forEach(function (valueList) {
            var val = entry[g.group][figure];
            if (val === undefined) {return;}
              if (valueList === self.dygraphConfig.differenceBasedLineChartType) {
              if (!self.LastValues[figure]) {
                self.LastValues[figure] = {value : val , time: 0};
              }
              if (!self.LastValues[figure].value) {
                self.LastValues[figure].value = val;
              }
              if (val !== null) {
                var graphVal = (val - self.LastValues[figure].value) /
                    ((time - self.LastValues[figure].time) / 1000 );
                valueLists[valueList].data.push(
                  [new Date(time), graphVal]
                );
                self.LastValues[figure] = {
                  value : val, 
                  time: time, 
                  graphVal : graphVal
                };
              } else {
                valueLists[valueList].data.push(
                  [new Date(time), null]
                );
                self.LastValues[figure] = {
                  value : undefined, 
                  time: 0, 
                  graphVal : null
                };
              }

            } else if (valueList === self.dygraphConfig.distributionChartType) {
              valueLists[valueList].data = val;
            } else if (valueList === self.dygraphConfig.regularLineChartType) {
              valueLists[valueList].data.push([new Date(time), val]);
            } else if (valueList === self.dygraphConfig.distributionBasedLineChartType)  {
                if (val !== null) {
                val = val.count === 0 ? 0 : val.sum / val.count;
              }
              self.LastValues[figure] = {value : val,  time: 0, graphVal : val};
              valueLists[valueList].data.push([
                new Date(time),
                val
              ]);
            }
          });
        });
      });
      Object.keys(self.dygraphConfig.combinedCharts).forEach(function (cc) {
        var part = cc.split("_");
        var val = [new Date(time)];
          self.dygraphConfig.combinedCharts[cc].sort().forEach(function(attrib) {
          if (self.LastValues[attrib])  {
              val.push(self.LastValues[attrib].graphVal);
          } 
          else if (typeof entry[part[0]] === 'object' && 
                   entry[part[0]].hasOwnProperty(attrib) &&
              (typeof entry[part[0]][attrib] === "number" || entry[part[0]][attrib] === null)) {
                val.push(entry[part[0]][attrib]);
          }
        });
        if (val.length > 1) {
          self.series[part[0]][part[1]].current.data.push(val);
        }
      });
    },

    calculateSeries: function () {
      var self = this;
      self.LastValues = {};
      self.maxMinValue = {};
      self.history.forEach(function(entry) {
        self.processSingleStatistic(entry);
      });

      Object.keys(self.dygraphConfig.combinedCharts).forEach(function (cc) {
        var part = cc.split("_");
        self.dygraphConfig.combinedCharts[cc].sort().forEach(function(attrib) {
          Object.keys(self.series[part[0]][attrib]).forEach(function(c) {
            self.series[part[0]][attrib][c].showGraph = false;
          });
        });
      });
    },


    updateSeries : function(data) {
      this.processSingleStatistic(data);
    },


    createLineChart : function (chart, figure, div, createDiv) {
      var self = this;
      var displayOptions = {};
      var t = new Date().getTime();
      var borderLeft, borderRight;
      if (!createDiv) {
          displayOptions.height = $('#lineChartDetail').height() - 34 -29;
          displayOptions.width = $('#lineChartDetail').width() -10;
          chart.detailChartConfig();
          if (chart.graph.dateWindow_) {
              borderLeft = chart.graph.dateWindow_[0];
              borderRight = t - chart.graph.dateWindow_[1] - self.interval * 5 > 0 ?
                  chart.graph.dateWindow_[1] : t;
          }
      } else {
          chart.updateDateWindow();
          borderLeft = chart.options.dateWindow[0];
          borderRight = chart.options.dateWindow[1];
      }
      if (self.modal && createDiv) {
          displayOptions.height = $('.innerDashboardChart').height() - 34;
          displayOptions.width = $('.innerDashboardChart').width() -45;
      }

      if (!chart.graphCreated) {
        if (createDiv) {
          self.renderHttpGroup(figure);
        }

        chart.updateDateWindow();
        chart.graph = new Dygraph(
          document.getElementById(div+"LineChart"),
          chart.data,
          _.extend(chart.options,  displayOptions)
        );
        chart.graphCreated = true;
        if (!createDiv) {
          self.currentChart = chart.graph;
          self.delegateEvents();
        }
      } else {
        var opts = {
            file: chart.data,
            dateWindow : [borderLeft, borderRight]
        };
        chart.graph.updateOptions(opts);
      }
    },

    resize: function() {
        var self = this;
        if (this.modal) {
            Object.keys(self.series).forEach(function(group) {
                Object.keys(self.series[group]).forEach(function(figure) {
                    Object.keys(self.series[group][figure]).forEach(function(valueList) {
                        var chart = self.series[group][figure][valueList];
                        if (chart.type === "current" && chart.showGraph === true &&
                            self.dygraphConfig.hideGraphs.indexOf(figure) === -1) {
                            var height = $('.innerDashboardChart').height() - 34;
                            var width = $('.innerDashboardChart').width() -45;
                            chart.graph.resize(width , height);
                        }
                    });
                });
            });
        } else if (self.currentChart) {
            var height = $('#lineChartDetail').height() - 34 - 29;
            var width = $('#lineChartDetail').width() - 10;
            self.currentChart.resize(width , height);
        }
    },

    createLineCharts: function() {
      var self = this;
      Object.keys(self.series).forEach(function(group) {
        Object.keys(self.series[group]).forEach(function(figure) {
          Object.keys(self.series[group][figure]).forEach(function(valueList) {
            var chart = self.series[group][figure][valueList];
            if (chart.type === "current" && chart.showGraph === true &&
              self.dygraphConfig.hideGraphs.indexOf(figure) === -1) {
              self.createLineChart(chart, figure, figure, true);
            }
          });
        });
      });
    },

    getStatisticHistory : function (params) {
      if (this.options.server) {
        params.server  = this.options.server;
      }
      this.documentStore.getStatisticsHistory(params);
      this.history = this.documentStore.history;
    },

    stopUpdating: function () {
      this.isUpdating = false;
    },

    startUpdating: function () {
      var self = this;
      if (self.isUpdating) {
        return;
      }
      self.isUpdating = true;
      self.timer = window.setInterval(function() {
        self.collection.fetch({
          url: self.statisticsUrl,
          success: function() {
            self.updateSeries({
              time : new Date().getTime() / 1000,
              client: self.collection.first().get("client"),
              http: self.collection.first().get("http"),
              server: self.collection.first().get("server"),
              system: self.collection.first().get("system")
            });
            if (!self.isUpdating) {
                return;
            }
            if (Object.keys(self.detailChart).length === 0) {
              self.createLineCharts();
              self.createDistributionCharts();
            } else {
              self.createLineChart(self.detailChart.chart,
                self.detailChart.figure, self.detailChart.div);
            }
          }
        });
      },
      self.interval
    );
  },

  showDetailFor : function (div, figure, chart, title) {
    this.detailChart = {
        div: div, chart : chart,
        figure: figure, graphCreated : false,
        title : title
    };
  },

  template: templateEngine.createTemplate("dashboardView.ejs"),

  httpTemplate: templateEngine.createTemplate("dashboardHttpGroup.ejs"),

  distributionTemplate: templateEngine.createTemplate("dashboardDistribution.ejs"),

  renderHttpGroup: function(id) {
    if ($('#' + id + "LineChart").length > 0) {
        return;
    }
    $(this.dygraphConfig.figureDependedOptions[id].div).append(this.httpTemplate.render({
      id : id + "LineChart"
    }));
  },

  render: function() {
    var self = this;
    var header = "Request Statistics";
    var addBackbutton = false;
    if (this.options.server) {
      header += " for Server ";
      header += this.options.server.raw + " (";
      header += decodeURIComponent(this.options.server.target) + ")";
    }
    $(this.el).html(this.template.render({
      header : header
    }));
    this.renderDistributionPlaceholder();
    this.prepareSeries();
    this.calculateSeries();
    this.createLineCharts();
    this.createDistributionCharts();
  },

  createDistributionSeries: function(name) {
    var cuts = [];

    _.each(this.description.attributes.figures, function(k, v) {
      if (k.identifier === name) {
        if (k.units == "bytes") {
            var c = 0;
            _.each(k.cuts, function() {
                k.cuts[c] = k.cuts[c] / 1000;
                c++;
            });
            k.units = "kilobytes";
        } else if (k.units == "seconds") {
            c = 0;
            _.each(k.cuts, function() {
                k.cuts[c] = k.cuts[c] * 1000;
                c++;
            });
            k.units = "milliseconds";
        }
        cuts = k.cuts;
        if (cuts[0] !== 0) {
          cuts.unshift(0);
        }
      }
    });

    //value prep for d3 graphs
    var distributionValues = this.series.client[name].distribution.data.counts;
    var sum = this.series.client[name].distribution.data.sum;
    var areaLength = this.series.client[name].distribution.data.counts.length;
    var values = [];
    var counter = 0;
    _.each(distributionValues, function() {
      values.push({
        //"label" : (sum / areaLength) * counter,
        "label" : counter === cuts.length-1 ? "> " + cuts[counter] : cuts[counter] +" - " + cuts[counter+1],
        "value" : distributionValues[counter]
      });
      counter++;
    });
    return values;
  },

  createDistributionCharts: function () {
    //distribution bar charts
    var self = this;
    _.each(this.dygraphConfig.chartTypeExceptions.distribution, function(k, v) {
      var title;
      var valueData = self.createDistributionSeries(v);
      _.each(self.description.attributes.figures, function(a, b) {
            if (a.identifier === v) {
                title = a.name + " in " + a.units;
            }
      });
      var data = [{
        key: title,
        color: "#617E2B",
        values: valueData
      }];
      nv.addGraph(function() {
        var chart = nv.models.multiBarHorizontalChart()
        .x(function(d) { return d.label; })
        .y(function(d) { return d.value; })
        //.margin({top: 30, right: 20, bottom: 50, left: 175})
        .margin({left: 80})
        .showValues(true)
        .showYAxis(false)
        .transitionDuration(350)
        .tooltips(false)
        .showLegend(false)
        .showControls(false);

        /*chart.yAxis
        .tickFormat(d3.format(',.2f'));*/
        chart.xAxis.showMaxMin(false);
        chart.yAxis.showMaxMin(false);
        /*chart.xAxis
        .tickFormat(d3.format(',.2f'));*/
        d3.select('#' + v + 'Distribution svg')
        .datum(data)
        .call(chart)
        .append("text")
        .attr("x", 0)
        .attr("y", 16)
        .style("font-size", "15px")
        .style("font-weight", 400)
        .style("font-family", "Open Sans")
        .text(title);

        nv.utils.windowResize(chart.update);
      });
    });
  },

  renderDistributionPlaceholder: function () {
    var self = this;
    _.each(this.dygraphConfig.chartTypeExceptions.distribution, function(k, v) {
      $(self.distributionChartDiv).append(self.distributionTemplate.render({
        elementId: v + "Distribution"
      }));
    });
  },

  returnToClusterView : function() {
    window.history.back();
  }
});
}());
