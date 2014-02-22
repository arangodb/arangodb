/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global templateEngine */

(function() {
  "use strict";

  window.dashboardView = Backbone.View.extend({
    el: '#content',
    contentEL: '.contentDiv',
    interval: 100000,
    defaultHistoryElements: 3, //in days
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

    stackedCharts : ["http_requests", "system_systemUserTime"],


    initialize: function () {
      this.arangoReplication = new window.ArangoReplication();
      this.documentStore = this.options.documentStore;
      this.getStatisticHistory();
      this.description = this.options.description.models[0];
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
            self.series[figure.group][figure.identifier] = {};
            if (self.chartTypeExceptions[figure.type] &&
                self.chartTypeExceptions[figure.type][figure.identifier]) {
                self.series[figure.group][figure.identifier][
                    self.chartTypeExceptions[figure.type][figure.identifier]
                    ] = [];

                if (figure.type === "distribution") {
                    self.series[figure.group][figure.identifier]["distribution"] = undefined;
                }
            } else {
                 self.series[figure.group][figure.identifier][figure.type] = [];
            }




        });
    },


    calculateSeries: function () {
        var self = this;
        self.LastValues = {};
        self.history.forEach(function(entry) {
            var time = entry.time * 1000;
            //iteration über Gruppen
            self.description.get("groups").forEach(function(g) {
                //iteration über figures
                Object.keys(entry[g.group]).forEach(function(figure) {
                   //iteration über valuelisten:
                   var valueLists = self.series[g.group][figure];
                    Object.keys(valueLists).forEach(function (valueList) {
                        var val = entry[g.group][figure];
                        if (valueList === "lineChartDiffBased") {
                            if (!self.LastValues[figure]) {
                                self.LastValues[figure] = val;
                            }
                            valueLists[valueList].push([new Date(time), (val - self.LastValues[figure]) / val]);
                            self.LastValues[figure] = val;
                        } else if (valueList === "distribution") {
                            valueLists[valueList] = val;
                        } else if (valueList === "accumulated") {

                        } else if (valueList === "current") {
                            valueLists[valueList].push([new Date(time), val]);
                        } else if (valueList === "currentDistribution")  {
                            valueLists[valueList].push([
                                new Date(time),
                                val.count === 0 ? 0 : val.sum / val.count
                            ]);
                        }
                    });

                });
            });
            Object.keys(self.combinedCharts).forEach(function (cc) {
                var part = cc.split("_");
                if (!self.series[part[0]][part[1]]) {
                    self.series[part[0]][part[1]] = {
                        current:  []
                    };
                }
                var val = [new Date(time)];
                self.combinedCharts[cc].sort().forEach(function(attrib) {
                    val.push(entry[part[0]][attrib]);
                })
                self.series[part[0]][part[1]]["current"].push(val);
            })
        });

        Object.keys(self.series).forEach(function(group) {
            Object.keys(self.series[group]).forEach(function(figure) {
                Object.keys(self.series[group][figure]).forEach(function(valueList) {
                    if (valueList === "lineChartDiffBased") {
                        self.series[group][figure]["current"] = self.series[group][figure][valueList];
                        delete self.series[group][figure][valueList];
                    } else if (valueList === "accumulated") {
                        delete self.series[group][figure][valueList];
                    } else if (valueList === "current") {

                    } else if (valueList === "distribution") {

                    } else if (valueList === "currentDistribution")  {
                        self.series[group][figure]["current"] = self.series[group][figure][valueList];
                        delete self.series[group][figure][valueList];
                    }
                });
            });
        });
        Object.keys(self.combinedCharts).forEach(function (cc) {
            var part = cc.split("_");
            self.combinedCharts[cc].sort().forEach(function(attrib) {
                delete self.series[part[0]][attrib];
            })
        })
        console.log(self.series);
        delete self.LastValues;
    },


    createLineChart: function() {
        if (!this.graphs["request"]) {
            console.log(this.series["http"]["requests"]["current"]);
            this.graphs["request"] = new Dygraph(
                document.getElementById("dashboardHttpGroup"),
                this.series["http"]["requests"]["current"],
                {   title: 'Average request time in milliseconds',
                    //yLabelWidth: "15",
                    //labelsDivStyles: { 'backgroundColor': 'transparent','textAlign': 'right' },
                    //hideOverlayOnMouseOut: true,
                    //labelsSeparateLines: true,
                    //legend: "always",
                    //labelsDivWidth: 150,
                    //labelsShowZeroValues: false,
                    //highlightSeriesBackgroundAlpha: 0.5,
                    //drawPoints: true,
                    //width: 480,
                    //height: 320,
                    labels: ["dateTime", "requestsGet","requestsHead",
                        "requestsPost","requestsPut",
                        "requestsPatch","requestsDelete",
                        "requestsOptions", "requestsOther"],
                    //visibility:getVisibility() ,
                    //valueRange: [self.min -0.1 * self.min, self.max + 0.1 * self.max],
                    //stackedGraph: false,
                    //axes: {
                    //  y: {
                    //        valueFormatter: function(y) {
                    //          return y.toPrecision(2);
                    //                          },
                    //      axisLabelFormatter: function(y) {
                    //          return y.toPrecision(2);
                    //      }
                    //  },
                    //      x: {
                    //      valueFormatter: function(d) {
                    //              if (d === -1) {return "";}
                    //              var date = new Date(d);
                    //              return Dygraph.zeropad(date.getHours()) + ":"
                    //                  + Dygraph.zeropad(date.getMinutes()) + ":"
                    //                  + Dygraph.zeropad(date.getSeconds());
                    //                          }
                    //  }
                    //                  },
                    //highlightCircleSize: 2,
                    //strokeWidth: 1,
                    //strokeBorderWidth: null,
                   highlightSeriesOpts: {
                        strokeWidth: 3,
                        strokeBorderWidth: 1,
                        highlightCircleSize: 5
                      }
                    });
                var onclick = function(ev) {
                    if (this.graphs["request"].isSeriesLocked()) {
                        this.graphs["request"].clearSelection();
                    } else {
                        this.graphs["request"].setSelection(this.graphs["request"].getSelection(), this.graphs["request"].getHighlightSeries(), true);
                    }
                };
                this.graphs["request"].updateOptions({clickCallback: onclick}, true);
                this.graphs["request"].setSelection(false, 'ClusterAverage', true);
             } else {

            this.graph.updateOptions( {
                'file': this.series["http"]["requests"]["current"]
            } );
        }
        }
    ,

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
                    self.history.push({
                        time : new Date().getTime() / 1000,

                        client: self.collection.first().get("client"),
                        http: self.collection.first().get("http"),
                        server: self.collection.first().get("server"),
                        system: self.collection.first().get("system")

                    });
                    self.prepareSeries();
                    self.calculateSeries();
                    self.renderFigures();
                    self.renderPieCharts();
                    self.createLineChart();
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

    renderHttpGroup: function() {
      $(this.contentEl).append(this.httpTemplate.render());
    },

    render: function() {
      var self = this;
      $(this.el).html(this.template.render({}));
      this.renderDistributionPlaceholder();
      this.prepareSeries();
      this.calculateSeries();
      this.renderFigures();
      this.renderPieCharts();
      this.createLineChart();

      this.renderHttpGroup();
      console.log(this.series);
    },

    renderDistributionPlaceholder: function () {
      var self = this;

      _.each(this.chartTypeExceptions.distribution(function(k, v) {
        console.log(k);
      }));
    }


  });
}()
);
