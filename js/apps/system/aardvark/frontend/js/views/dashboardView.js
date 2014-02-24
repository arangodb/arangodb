/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global templateEngine */

(function() {
  "use strict";
   var formatBytes = function (y) {
        if (y > 100000000) {
            y = y / 1000000000;
            return y.toPrecision(3) + "GB";
        } else if (y > 100000)  {
            y = y / 100000;
            return y.toPrecision(3) + "MB";
        } else if (y > 100) {
            y = y / 1000;
            return y.toPrecision(3) + "KB";
        } else  {
           return y.toPrecision(3) + "B";
       }
  };
  var formatSeconds = function (y) {
    //  return y * 1000 + "ms";
      return y
  };
  var zeropad = function(x) {
     if (x < 10) return "0" + x; else return "" + x;
  };

  var xAxisFormat = function (d) {
    if (d === -1) {return "";}
    var date = new Date(d);
    return zeropad(date.getHours()) + ":"
        + zeropad(date.getMinutes()) + ":"
        + zeropad(date.getSeconds());
  };

  var mergeObjects = function(o1, o2, mergeAttribList) {
      if (!mergeAttribList) {mergeAttribList = [];}
      var vals = {};
      mergeAttribList.forEach(function (a) {
        var valO1 = o1[a];
        var valO2 = o2[a];
        valO1 === undefined ? valO1 = {} :null ;
        valO2 === undefined ? valO2 = {} :null ;
        vals[a] = _.extend(valO1, valO2);
      });
      var res = _.extend(o1,o2);
      Object.keys(vals).forEach(function(k) {
          res[k] = vals[k];
      })
      return res;
  }

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
    colors : ["#617e2b", "#cad4b8", "#617e2b", "#40541c", "#202a0e", "#767676", "#b8860b","#4779f4"],




    figureDependedOptions : {
        numberOfThreads : {
            sigFigs : 0
        },
        residentSize : {
        },
        virtualSize : {
        },
        minorPageFaults : {},
        majorPageFaults : {},
        systemUserTime : {
            title : "System and User Time",
            stacked : true
        },
        httpConnections : {},
        totalTime : {
            axes: {
                y: {
                    ticker: Dygraph.numericLinearTicks,
                    valueFormatter: function(y) {
                        return formatSeconds(y);
                    },
                    axisLabelFormatter: function(y) {
                        return formatSeconds(y);
                    }
                }
            }
        },
        requestTime : {
            axes: {
                y: {
                    ticker: Dygraph.numericLinearTicks,
                    valueFormatter: function(y) {
                        return formatSeconds(y);
                    },
                    axisLabelFormatter: function(y) {
                        return formatSeconds(y);
                    }
                }
            }
        },
        queueTime : {
            axes: {
                y: {
                    ticker: Dygraph.numericLinearTicks,
                    valueFormatter: function(y) {
                        return formatSeconds(y);
                    },
                    axisLabelFormatter: function(y) {
                        return formatSeconds(y);
                    }
                }
            }
        },
        bytesSent : {
            axes: {
            }
        },
        bytesReceived : {
            axes: {
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
            labelsSeparateLines: true,
            fillGraph : true,
            strokeWidth: 3,
            colors: [this.colors[0]],
            axes : {
                x: {
                    valueFormatter: function(d) {
                        return xAxisFormat(d);
                    }
                },
                y: {
                    labelsKMG2: true,
                    ticker: Dygraph.numericLinearTicks
                }
            }
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
            var colors = self.colors.concat([]);
            self.combinedCharts[cc].sort().forEach(function(attrib) {
                label.push(attrib);
            })
            self.getChartsForFigure(fig);
            console.log(colors.slice(0, label.length));
            self.series[fig.group][fig.identifier]["current"]["options"]["colors"] = colors.slice(0, label.length);
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
                    if (valueLists[valueList]["data"] && valueList !== "distribution") {
                        var v = valueLists[valueList]["data"][valueLists[valueList]["data"].length-1];
                        if (!self.maxMinValue[figure]) {
                            self.maxMinValue[figure] = {
                                max: v,
                                min: v
                            };
                        } else {
                            self.maxMinValue[figure] = {
                                max: v >=  self.maxMinValue[figure] ? v :  self.maxMinValue[figure],
                                min: v <= self.maxMinValue[figure] ? v :  self.maxMinValue[figure]
                            }
                        }
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
        self.maxMinValue = {};
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
                            console.log(chart["data"]);
                            console.log(chart["options"]);
                            self.renderHttpGroup(figure);
                            chart["graph"] = new Dygraph(
                                document.getElementById(figure+"LineChart"),
                                chart["data"],
                                mergeObjects(
                                    chart["options"],
                                    self.figureDependedOptions[figure],
                                    ["axes"])
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
      var cuts = [];

      _.each(this.description.attributes.figures, function(k, v) {
        if (k.identifier === name) {
          cuts = k.cuts;
          cuts.unshift(0);
        }
      });

      //value prep for d3 graphs
      var distributionValues = this.series.client[name].distribution.data.counts;
      var sum = this.series.client[name].distribution.data.sum;
      var areaLength = this.series.client[name].distribution.data.counts.length;
      var values = [];
      var counter = 1;

      _.each(distributionValues, function() {
        values.push({
          //"label" : (sum / areaLength) * counter,
          "label" : cuts[counter-1],
          "value" : distributionValues[counter-1]
        });
        counter++;
      });
      return values;
    },

    createDistributionCharts: function () {
      //distribution bar charts
      var self = this;
      _.each(this.chartTypeExceptions.distribution, function(k, v) {

        var valueData = self.createDistributionSeries(v);

        var data = [{
          key: v,
          color: "#617E2B",
          values: valueData
        }];

        nv.addGraph(function() {
          var chart = nv.models.multiBarHorizontalChart()
          .x(function(d) { return d.label })
          .y(function(d) { return d.value })
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
