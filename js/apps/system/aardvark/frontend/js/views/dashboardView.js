/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global templateEngine */

(function() {
  "use strict";

  window.dashboardView = Backbone.View.extend({
    el: '#content',
    contentEl: '.contentDiv',
    interval: 100000,
    defaultHistoryElements: 1 / 1000, //in days
    chartTypeExceptions : {
        accumulated : {
            minorPageFaults : "lineChartDiffBased",
            majorPageFaults : "lineChartDiffBased",
            requestsTotal : "lineChartDiffBased",
            requestsAsync: "lineChartDiffBased",
            requestsGet: "lineChartDiffBased",
            requestsHead: "lineChartDiffBased",
            requestsPost: "lineChartDiffBased",
            requestsPut: "lineChartDiffBased",
            requestsPatch: "lineChartDiffBased",
            requestsDelete: "lineChartDiffBased",
            requestsOptions: "lineChartDiffBased",
            requestsOther: "lineChartDiffBased"

        },

        distribution : {
            totalTime : "currentDistribution",
            requestTime: "currentDistribution",
            queueTime: "currentDistribution",
            bytesSent: "currentDistribution",
            bytesReceived: "currentDistribution"
        }
    },
    combinedCharts : {
        http_requests: [
            "requestsGet","requestsHead",
            "requestsPost","requestsPut",
            "requestsPatch","requestsDelete",
            "requestsOptions", "requestsOther"
        ],
        system_systemUserTime: ["systemTime","userTime"]
    },

    figureDependedOptions : {
        numberOfThreads : {},
        residentSize : {
            axes: {
                y: {
                    valueFormatter: function(y) {
                        y = y / 1000000;
                        return y.toPrecision(2) + "MB";

                    },
                    axisLabelFormatter: function(y) {
                        y = y / 1000000;
                        return y.toPrecision(2) + "MB";
                    }
                }
            }
        },
        virtualSize : {
            axes: {
                y: {
                    valueFormatter: function(y) {
                        y = y / 1000000000;
                        return y.toPrecision(2) + "GB";

                    },
                    axisLabelFormatter: function(y) {
                        y = y / 1000000000;
                        return y.toPrecision(2) + "GB";
                    }
                }
            }
        },
        minorPageFaults : {},
        majorPageFaults : {},
        systemUserTime : {
            title : "System and User Time",
            stacked : true
        },
        httpConnections : {},
        totalTime : {},
        requestTime : {},
        queueTime : {},
        bytesSent : {
            axes: {
                y: {
                    valueFormatter: function(y) {
                        y = y / 1000000;
                        return y.toPrecision(2) + "MB";

                    },
                    axisLabelFormatter: function(y) {
                        y = y / 1000000;
                        return y.toPrecision(2) + "MB";
                    }
                }
            }
        },
        bytesReceived : {
            axes: {
                y: {
                    valueFormatter: function(y) {
                        y = y / 1000000;
                        return y.toPrecision(2) + "MB";

                    },
                    axisLabelFormatter: function(y) {
                        y = y / 1000000;
                        return y.toPrecision(2) + "MB";
                    }
                }
            }
        },
        requestsTotal : {},
        requestsAsync : {},
        requests : {
            title : "HTTP Requests",
            stacked : true
        },
        uptime : {}
    },


    initialize: function () {
      this.arangoReplication = new window.ArangoReplication();
      this.documentStore = this.options.documentStore;
      this.getStatisticHistory();
      this.description = this.options.description.models[0];
      this.startUpdating();
      this.graphs = {};
     },



    getChartsForFigure : function (figure) {
        this.series[figure.group][figure.identifier] = {};
        var result = {};
        if (this.chartTypeExceptions[figure.type] &&
            this.chartTypeExceptions[figure.type][figure.identifier]) {
            var d = {
                type: this.chartTypeExceptions[figure.type][figure.identifier],
                identifier: figure.identifier,
                group: figure.group,
                name : figure.name
            };

            result[this.chartTypeExceptions[figure.type][figure.identifier]] =
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
        var options = {
            labelsDivStyles: { 'backgroundColor': 'transparent','textAlign': 'right' },
            labelsSeparateLines: true
        };
        if (figure.name) {
            options["title"] =  figure.name;
        }
        var type = figure.type;
        var showGraph = true;
        if (type === "lineChartDiffBased") {
            options["title"] +=  " per seconds";
            type = "current";
        } else if (type === "distribution") {
            type = "distribution";
        } else if (type === "accumulated") {
            showGraph = false;
        } else if (type === "currentDistribution")  {
            type = "current";
        }
        if (type === "current") {
            options["labels"] = ["datetime" , figure.identifier];
        }
        return {
            type : type,
            showGraph: showGraph,
            data: type === "distribution" ? undefined  : [],
            options : options,
            graph : undefined
        };
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
        Object.keys(self.combinedCharts).forEach(function (cc) {
            var part = cc.split("_");
            var fig = {identifier : part[1], group : part[0], type : "current"};
            var label = ["datetime"];
            self.combinedCharts[cc].sort().forEach(function(attrib) {
                label.push(attrib);
            })
            self.getChartsForFigure(fig);
            self.series[fig.group][fig.identifier]["current"]["options"]["labels"] = label;
         });

    },


    processSingleStatistic: function (entry) {
        var self = this;
        var time = entry.time * 1000;
        self.description.get("groups").forEach(function(g) {
            Object.keys(entry[g.group]).forEach(function(figure) {
                var valueLists = self.series[g.group][figure];
                Object.keys(valueLists).forEach(function (valueList) {
                    var val = entry[g.group][figure];
                    if (valueList === "lineChartDiffBased") {
                        if (!self.LastValues[figure]) {
                            self.LastValues[figure] = {value : val , time: 0};
                        }
                        val = (val - self.LastValues[figure]["value"]) / (time - self.LastValues[figure]["time"]);
                        valueLists[valueList]["data"].push(
                            [new Date(time), val]
                        );
                        self.LastValues[figure] = {value : val , time: time};

                    } else if (valueList === "distribution") {
                        valueLists[valueList]["data"] = val;
                    } else if (valueList === "accumulated") {

                    } else if (valueList === "current") {
                        valueLists[valueList]["data"].push([new Date(time), val]);
                    } else if (valueList === "currentDistribution")  {
                        valueLists[valueList]["data"].push([
                            new Date(time),
                            val.count === 0 ? 0 : val.sum / val.count
                        ]);
                    }
                });

            });
        });
        Object.keys(self.combinedCharts).forEach(function (cc) {
            var part = cc.split("_");
            var val = [new Date(time)];
            self.combinedCharts[cc].sort().forEach(function(attrib) {
                if (self.LastValues[attrib])  {
                    val.push(self.LastValues[attrib]["value"]);
                } else {
                    val.push(entry[part[0]][attrib]);
                }
            })
            self.series[part[0]][part[1]]["current"]["data"].push(val);
        })
    },

    calculateSeries: function () {
        var self = this;
        self.LastValues = {};
        self.history.forEach(function(entry) {
            self.processSingleStatistic(entry);
        });

        Object.keys(self.combinedCharts).forEach(function (cc) {
            var part = cc.split("_");
            self.combinedCharts[cc].sort().forEach(function(attrib) {
                Object.keys(self.series[part[0]][attrib]).forEach(function(c) {
                    self.series[part[0]][attrib][c]["showGraph"] = false;
                })
            })
        })
    },


    updateSeries : function(data) {
        console.log(data);
        this.processSingleStatistic(data);
    },


    createLineCharts: function() {
        var self = this;
        console.log(self.series);
        Object.keys(self.series).forEach(function(group) {
            Object.keys(self.series[group]).forEach(function(figure) {
                Object.keys(self.series[group][figure]).forEach(function(valueList) {
                    var chart = self.series[group][figure][valueList];
                    if (chart["type"] === "current" && chart["showGraph"] === true) {
                        if (!chart["graph"]) {
                            console.log(figure);
                            self.renderHttpGroup(figure);
                            chart["graph"] = new Dygraph(
                                document.getElementById(figure+"LineChart"),
                                chart["data"],
                                _.extend(
                                chart["options"],
                                    self.figureDependedOptions[figure])
                                );
                                var onclick = function(ev) {
                                    if (chart["graph"].isSeriesLocked()) {
                                        chart["graph"].clearSelection();
                                    } else {
                                        chart["graph"].setSelection(chart["graph"].getSelection(), chart["graph"].getHighlightSeries(), true);
                                    }
                                };
                                chart["graph"].updateOptions({clickCallback: onclick}, true);
                                chart["graph"].setSelection(false, 'ClusterAverage', true);
                             } else {

                             chart["graph"].updateOptions( {
                                'file': chart["data"]
                                 } );
                        }
                    }
                });
            });
        });
    },

    renderFigures: function () {

    },

    renderPieCharts: function () {

    },

    renderLineChart: function () {

    },

    getStatisticHistory : function () {
        this.documentStore.getStatisticsHistory(
            (new Date().getTime() - this.defaultHistoryElements * 86400 * 1000) / 1000
        );

        this.history = this.documentStore.history;
    },

    startUpdating: function () {
        var self = this;
        if (this.isUpdating) {
            return;
        }
        this.isUpdating = true;
        var self = this;
        this.timer = window.setInterval(function() {
                self.collection.fetch({
                success: function() {
                    self.updateSeries({
                        time : new Date().getTime() / 1000,

                        client: self.collection.first().get("client"),
                        http: self.collection.first().get("http"),
                        server: self.collection.first().get("server"),
                        system: self.collection.first().get("system")

                    });
                    self.renderFigures();
                    self.renderPieCharts();
                    self.createLineCharts();
                },
                 error: function() {

                  }
                });
            },
            self.interval
        );
    },

    template: templateEngine.createTemplate("dashboardView.ejs"),

    httpTemplate: templateEngine.createTemplate("dashboardHttpGroup.ejs"),

    distributionTemplate: templateEngine.createTemplate("dashboardDistribution.ejs"),

    renderHttpGroup: function(id) {
      $(this.contentEl).append(this.httpTemplate.render({id : id+"LineChart"}));
    },

    render: function() {
      var self = this;
      $(this.el).html(this.template.render({}));
      this.renderDistributionPlaceholder();
      this.prepareSeries();
      this.calculateSeries();
      this.renderFigures();
      this.renderPieCharts();
      this.createLineCharts();
      this.createDistributionCharts();
    },

    createDistributionSeries: function(name) {
      console.log(name);
      //value prep for d3 graphs
      var distributionValues = this.series.client[name].distribution.data.counts;
      var sum = this.series.client[name].distribution.data.sum;
      var areaLength = this.series.client[name].distribution.data.counts.length;
      var values = [];
      var counter = 1;

      _.each(distributionValues, function() {
        values.push({
          "label" : (sum / areaLength) * counter,
          "value" : distributionValues[counter-1]
        });
        counter++;
      });
      console.log(values);
    return values;
    },

    createDistributionCharts: function () {
      //distribution bar charts
      var self = this;
      _.each(this.chartTypeExceptions.distribution, function(k, v) {

        var valueData = self.createDistributionSeries(v);

        var data = [{
          key: v,
          color: "#D67777",
          values: valueData
        }];

        nv.addGraph(function() {
          var chart = nv.models.multiBarHorizontalChart()
          .x(function(d) { return d.label })
          .y(function(d) { return d.value })
          //.margin({top: 30, right: 20, bottom: 50, left: 175})
          .showValues(true) //Show bar value next to each bar.
          .tooltips(true) //Show tooltips on hover.
          .transitionDuration(350)
          .showControls(true); //Allow user to switch between "Grouped" and "Stacked" mode.

          chart.yAxis
          .tickFormat(d3.format(',.2f'));

          chart.xAxis
          .tickFormat(d3.format(',.2f'));

          d3.select('#' + v + ' svg')
          .datum(data)
          .call(chart)
          .append("text")
          .attr("x", 0)
          .attr("y", 16)
          .style("font-size", "16px")
          .style("text-decoration", "underline")
          .text(v);

          nv.utils.windowResize(chart.update);
          return chart;
        });
      });
    },

    renderDistributionPlaceholder: function () {
      var self = this;
      _.each(this.chartTypeExceptions.distribution, function(k, v) {
        $(self.contentEl).append(self.distributionTemplate.render({elementId: v+"Distribution"}));
      });
    }


  });
}()
);
