/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.dashboardView = Backbone.View.extend({
    el: '#content',
    contentEl: '.contentDiv',
    distributionChartDiv : "#distributionChartDiv",
    interval: 3000, // in milliseconds
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
      var chart;
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
      self.showDetailFor("dashboardDetailed", id, chart);
      self.createLineChart(chart, id,  "dashboardDetailed");
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
      this.description = this.options.description.models[0];
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



    getChartsForFigure : function (figure) {
      this.series[figure.group][figure.identifier] = {};
      var result = {};
      if (this.dygraphConfig.chartTypeExceptions[figure.type] &&
        this.dygraphConfig.chartTypeExceptions[figure.type][figure.identifier]) {
        var d = {
          type: this.dygraphConfig.chartTypeExceptions[figure.type][figure.identifier],
          identifier: figure.identifier,
          group: figure.group,
          name : figure.name
        };

        result[this.dygraphConfig.chartTypeExceptions[figure.type][figure.identifier]] =
        this.getChartStructure(d);
        if (figure.type === "distribution") {
          result[figure.type] = this.getChartStructure(figure);
        }
      } else {
        result[figure.type] = this.getChartStructure(figure);
      }
      this.series[figure.group][figure.identifier] =  result;
    },

    getChartStructure: function (figure) {
      var title = "";
      if (figure.name) {
        title =  figure.name;
      }
      var type = figure.type;
      var showGraph = true;
      if (type === "lineChartDiffBased") {
        title +=  " per seconds";
        type = "current";
      } else if (type === "distribution") {
        type = "distribution";
      } else if (type === "accumulated") {
        showGraph = false;
      } else if (type === "currentDistribution")  {
        type = "current";
      }
      return new this.dygraphConfig.Chart(figure.identifier, type, showGraph, title);
    },

    prepareSeries: function () {
      var self = this;
      self.series = {};
      self.description.get("groups").forEach(function(group) {
        self.series[group.group] = {};
      });
      self.description.get("figures").forEach(function(figure) {
        self.getChartsForFigure(figure);
      });
      Object.keys(self.dygraphConfig.combinedCharts).forEach(function (cc) {
        var part = cc.split("_");
        var fig = {identifier : part[1], group : part[0], type : "current"};
        self.getChartsForFigure(fig);
      });
    },


    processSingleStatistic: function (entry) {
      var self = this;
      var time = entry.time * 1000;
      var newUptime = entry.server.uptime;
      if (self.uptime && newUptime < self.uptime) {

        var e = {time : (time-(newUptime+10)* 1000 ) /1000};
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
            if (valueList === "lineChartDiffBased") {
              if (!self.LastValues[figure]) {
                self.LastValues[figure] = {value : val , time: 0};
              }
              if (!self.LastValues[figure].value) {
                self.LastValues[figure].value = val;
              }
              if (val !== null) {
                var graphVal = (val - self.LastValues[figure].value) /
                               (time - self.LastValues[figure].time);
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

            } else if (valueList === "distribution") {
              valueLists[valueList].data = val;
            } else if (valueList === "current") {
              valueLists[valueList].data.push([new Date(time), val]);
            } else if (valueList === "currentDistribution")  {
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
          } else if (entry[part[0]] && entry[part[0]][attrib]) {
            val.push(entry[part[0]][attrib]);
          }
        });
        self.series[part[0]][part[1]].current.data.push(val);
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
      this.uptime = data.server.uptime;
      this.processSingleStatistic(data);
    },

    spliceSeries: function (data, boarderLeft, boarderRight, hideRangeSelector) {
      var file = [];
      var i = 0;
      data.forEach(function(e) {
        if (e[0].getTime()  >= boarderLeft && e[0].getTime() <= boarderRight) {
          file.push(e);
        } else {
          i++;
          if (i > 3 && !hideRangeSelector) {
            file.push(e);
            i = 0;
          }
        }
      });
      return file;
    },

    createLineChart : function (chart, figure, div, createDiv) {
      var self = this;
      var displayOptions = {};
      var file = [];
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
              file = self.spliceSeries(chart.data, borderLeft, borderRight, false);
          }
      } else {
          borderLeft = chart.options.dateWindow[0] + (t - chart.options.dateWindow[1]);
          borderRight = t;
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
          file.length > 0 ? file : chart.data,
          _.extend(chart.options,  displayOptions)
        );
        chart.graphCreated = true;
        if (!createDiv) {
          self.currentChart = chart.graph;
          self.delegateEvents();
        }
      } else {
        var opts = {
            file: file.length > 0 ? file : chart.data,
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
            if (!self.isUpdating) {
              return;
            }
            self.updateSeries({
              time : new Date().getTime() / 1000,
              client: self.collection.first().get("client"),
              http: self.collection.first().get("http"),
              server: self.collection.first().get("server"),
              system: self.collection.first().get("system")
            });
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

  showDetailFor : function (div, figure, chart) {
    this.detailChart = {div: div, chart : chart, figure: figure, graphCreated : false };
  },

  template: templateEngine.createTemplate("dashboardView.ejs"),

  httpTemplate: templateEngine.createTemplate("dashboardHttpGroup.ejs"),

  distributionTemplate: templateEngine.createTemplate("dashboardDistribution.ejs"),

  renderHttpGroup: function(id) {
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
        "label" : cuts[counter],
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

      var valueData = self.createDistributionSeries(v);

      var data = [{
        key: v,
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
        .transitionDuration(350)
        .tooltips(false)
        .showControls(false);

        chart.yAxis
        .tickFormat(d3.format(',.2f'));

        chart.xAxis
        .tickFormat(d3.format(',.2f'));

        d3.select('#' + v + 'Distribution svg')
        .datum(data)
        .call(chart)
        .append("text")
        .attr("x", 0)
        .attr("y", 16)
        .style("font-size", "16px")
        .style("text-decoration", "underline")
        .text(v);

        nv.utils.windowResize(chart.update);
      });
    });
  },

  renderDistributionPlaceholder: function () {
    var self = this;
      console.log(this);
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
