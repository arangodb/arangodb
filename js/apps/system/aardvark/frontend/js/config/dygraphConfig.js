/**
 * Created by fbartels on 05.03.14.
 */

(function() {
    "use strict";

    var zeropad = function(x) {
        if (x < 10) {
            return "0" + x;
        }
        return x;
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
            if (valO1 === undefined) {
                valO1 = {};
            }
            if (valO2 === undefined) {
                valO2 = {};
            }
            vals[a] = _.extend(valO1, valO2);
        });
        var res = _.extend(o1,o2);
        Object.keys(vals).forEach(function(k) {
            res[k] = vals[k];
        });
        return res;
    };

    // graphs that should never show
    var hideGraphs = [
        "totalTime",
        "uptime",
        "residentSize",
        "physicalMemory",
        "minorPageFaults",
        "requestsTotal"
    ];

    var isCombinedChart = function (chart) {
        var result = false;
        Object.keys(combinedCharts).forEach(function (cc) {
            var part = cc.split("_");
            if (part[1] === chart.figure) {
                chart.key = cc;
                result = true;
            };
        });
        return result;
    };


    var differenceBasedLineChartType = "lineChartDiffBased";
    var regularLineChartType = "current";
    var distributionChartType = "distribution";
    var accumulatedChartType = "accumulated";
    var distributionBasedLineChartType = "currentDistribution";

    // different time series types for charts
    var chartTypeExceptions = {};
    chartTypeExceptions[accumulatedChartType] = {
            minorPageFaults : differenceBasedLineChartType,
            majorPageFaults : differenceBasedLineChartType,
            requestsTotal : differenceBasedLineChartType,
            requestsAsync: differenceBasedLineChartType,
            requestsGet: differenceBasedLineChartType,
            requestsHead: differenceBasedLineChartType,
            requestsPost: differenceBasedLineChartType,
            requestsPut: differenceBasedLineChartType,
            requestsPatch: differenceBasedLineChartType,
            requestsDelete: differenceBasedLineChartType,
            requestsOptions: differenceBasedLineChartType,
            requestsOther: differenceBasedLineChartType

    };
    chartTypeExceptions[distributionChartType] = {
            totalTime : distributionBasedLineChartType,
            requestTime: distributionBasedLineChartType,
            queueTime: distributionBasedLineChartType,
            bytesSent: distributionBasedLineChartType,
            bytesReceived: distributionBasedLineChartType
    };
    // figures supposed to be dispalyed in one chart
    var combinedCharts = {
        http_requests: [
            "requestsGet","requestsHead",
            "requestsPost","requestsPut",
            "requestsPatch","requestsDelete",
            "requestsOptions", "requestsOther"
        ],
        system_systemUserTime: ["systemTime","userTime"],
        client_totalRequestTime: ["requestTime","queueTime"]
    };

    //colors for dygraphs
    var colors = ["#617e2b", "#296e9c", "#81ccd8", "#7ca530", "#3c3c3c",
        "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"];

        //figure dependend options
    var  figureDependedOptions = {
        numberOfThreads : {
            div : "#systemResources"
        },
        residentSize : {
            div : "#systemResources"
        },
        residentSizePercent : {
            div : "#systemResources",
                axes : {
                y: {
                    labelsKMG2: false,
                    axisLabelFormatter: function(y) {
                        return y.toPrecision(2) + "%";
                    },
                    valueFormatter: function(y) {
                        return y.toPrecision(2) + "%";
                    }
                }
            }
        },
        physicalMemory : {
            div : "#systemResources"
        },
        virtualSize : {
            div : "#systemResources"
        },
        minorPageFaults : {
            div : "#systemResources",
                labelsKMG2: false
        },
        majorPageFaults : {
            div : "#systemResources",
                labelsKMG2: false
        },
        systemUserTime : {
            div : "#systemResources",
            title : "System and User Time in seconds",
            stacked : true
        },
        httpConnections : {
            div : "#requestStatistics"
        },
        totalTime : {
            div : "#requestStatistics",
            title : "Total time in milliseconds",
            labelsKMG2: false,
            axes : {
                y: {
                    valueFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    },
                    axisLabelFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    }
                }
            }
        },
        requestTime : {
            div : "#requestStatistics",
            title : "Request time in milliseconds",
            labelsKMG2: false,
            axes : {
                y: {
                    valueFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    },
                    axisLabelFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    }
                }
            }
        },
        queueTime : {
            div : "#requestStatistics",
            title : "Queue time in milliseconds",
            labelsKMG2: false,
            axes : {
                y: {
                    valueFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    },
                    axisLabelFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    }
                }
            }
        },
        totalRequestTime : {
            div : "#requestStatistics",
            title : "Total time in milliseconds",
            labelsKMG2: false,
            axes : {
                y: {
                    valueFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    },
                    axisLabelFormatter: function(y) {
                        y = y * 1000;
                        return y.toPrecision(3);
                    }
                }
            }
        },
        bytesSent : {
            div : "#requestStatistics"
        },
        bytesReceived : {
            div : "#requestStatistics"
        },
        requestsTotal : {
            div : "#requestStatistics"
        },
        clusterAverageRequestTime : {
            showLabelsOnHighlight : true
        },
        requestsAsync : {
            div : "#requestStatistics"
        },
        requests : {
            div : "#requestStatistics",
                title : "HTTP Requests per second",
                stacked : true
        },
        uptime : {
            div : "#systemResources",
            axes : {
                y: {
                    labelsKMG2: false,
                        axisLabelFormatter: function(y) {
                        if (y < 60) {
                            return y.toPrecision(3) + "s";
                        }
                        if (y < 3600) {
                            y = y/60;
                            return y.toPrecision(3) + "m";
                        }
                        y = y/3600;
                        return y.toPrecision(3) + "h";
                    }
                }
            }
        }
    };

    //configuration for chart overview
    var getDefaultConfig = function(chart) {
        chart.options =  {
            digitsAfterDecimal: 2,
            drawGapPoints: true,
            fillGraph : true,
            showLabelsOnHighlight : false,
            strokeWidth: 2,
            strokeBorderWidth: 1,
            highlightCircleSize: 0,
            strokeBorderColor: '#ffffff',
            interactionModel :  {},
            dateWindow : [new Date().getTime() -
                Math.min(
                    20 * 60 * 1000,
                    chart.data.length * 10 * 1000)
                ,new Date().getTime()],
            colors: [colors[0]],
            xAxisLabelWidth : "60",
            rightGap: 10,
            showRangeSelector: false,
            rangeSelectorHeight: 40,
            rangeSelectorPlotStrokeColor: '#365300',
            rangeSelectorPlotFillColor: '#414a4c',
            pixelsPerLabel : 60,
            labelsKMG2: true,
            axes : {
                x: {
                    valueFormatter: function(d) {
                        return xAxisFormat(d);
                    }
                },
                y: {
                    ticker: Dygraph.numericLinearTicks
                }
            }
        };
        if (figureDependedOptions[chart.figure]) {
            chart.options =  mergeObjects(
                chart.options, figureDependedOptions[chart.figure], ["axes"]
            );
        }

    };

    var getDetailChartConfig = function (chart) {
        chart.options =  _.extend(
            chart.options,
            {
                showRangeSelector : true,
                title : "",
                interactionModel : null,
                showLabelsOnHighlight : true,
                highlightCircleSize : 3
            }
        );
    };

    var updateDateWindow = function(chart) {
        chart.options.dateWindow =   [new Date().getTime() -
                    20 * 60 * 1000
                ,new Date().getTime()];
    };
    window.dygraphConfig = {


        Chart : function (figure, type, showGraph, title) {
                this.type = type;
                this.showGraph = showGraph;
                this.data = type === distributionChartType ? undefined  : [];
                this.figure =  figure;
                this.options =  {};
                this.defaultConfig = function() {
                    getDefaultConfig(this);
                };
                this.detailChartConfig = function() {
                    getDetailChartConfig(this);
                };
                this.updateDateWindow = function() {
                    updateDateWindow(this);
                };
                this.isCombinedChart = function() {
                    return isCombinedChart(this);
                };
                var self = this;
                this.updateLabels = function (labels) {
                    if (labels) {
                        self.options.labels = labels;
                        var colorList = colors.concat([]);
                        self.options.colors = colorList.slice(0, self.options.labels.length-1);
                        return;
                    }
                    var labels = ["datetime"];
                    if (self.isCombinedChart()) {
                        combinedCharts[self.key].sort().forEach(function(attrib) {
                            labels.push(attrib);
                        });
                        self.options.labels = labels;
                        var colorList = colors.concat([]);
                        self.options.colors = colorList.slice(0, self.options.labels.length-1);
                    } else {
                        self.options.labels = ["datetime", self.figure];
                        self.options.colors = [colors[0]];
                    }
                };
            if (type !== distributionChartType) {
                this.defaultConfig();
                this.updateLabels();
                if (!this.options.title) {
                    this.options.title = title;
                }
            }
        },

        getChartsForFigure : function (figure, type, title) {
            var result = {};
            if (chartTypeExceptions[type] &&
                chartTypeExceptions[type][figure]) {
                result[chartTypeExceptions[type][figure]] =
                    this.getChartStructure(
                        figure,
                        chartTypeExceptions[type][figure],
                        title
                    );
                if (type === distributionChartType) {
                    result[type] = this.getChartStructure(figure, type, title);
                }
            } else {
                result[type] = this.getChartStructure(figure, type, title);
            }
            return result;
        },

        getChartStructure: function (figure, type, title) {
            var showGraph = true;
            if (type === differenceBasedLineChartType) {
                title +=  " per seconds";
                type = regularLineChartType;
            } else if (type === accumulatedChartType) {
                showGraph = false;
            } else if (type === distributionBasedLineChartType)  {
                type = regularLineChartType;
            }
            return new this.Chart(figure, type, showGraph, title);
        },


        chartTypeExceptions : chartTypeExceptions,

        combinedCharts : combinedCharts,

        hideGraphs: hideGraphs,

        figureDependedOptions: figureDependedOptions,

        differenceBasedLineChartType : differenceBasedLineChartType,
        regularLineChartType : regularLineChartType,
        distributionChartType : distributionChartType,
        accumulatedChartType : accumulatedChartType,
        distributionBasedLineChartType : distributionBasedLineChartType

    };
}());
