/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, continue: true, regexp: true */
/*global require, _, Dygraph, window, document */

(function () {
    "use strict";
    window.newDygraphConfig = {

        defaultFrame : 20 * 60 * 1000,

        zeropad: function (x) {
            if (x < 10) {
                return "0" + x;
            }
            return x;
        },

        xAxisFormat: function (d) {
            if (d === -1) {
                return "";
            }
            var date = new Date(d);
            return this.zeropad(date.getHours()) + ":"
                + this.zeropad(date.getMinutes()) + ":"
                + this.zeropad(date.getSeconds());
        },

        mergeObjects: function (o1, o2, mergeAttribList) {
            if (!mergeAttribList) {
                mergeAttribList = [];
            }
            var vals = {}, res;
            mergeAttribList.forEach(function (a) {
                var valO1 = o1[a],
                    valO2 = o2[a];
                if (valO1 === undefined) {
                    valO1 = {};
                }
                if (valO2 === undefined) {
                    valO2 = {};
                }
                vals[a] = _.extend(valO1, valO2);
            });
            res = _.extend(o1, o2);
            Object.keys(vals).forEach(function (k) {
                res[k] = vals[k];
            });
            return res;
        },

        mapStatToFigure : {
            numberOfThreads : ["times", "numberOfThreadsCurrent"],
            residentSize : ["times", "residentSizePercent"],
            virtualSize : ["times", "virtualSize"],
            pageFaults : ["times", "majorPageFaultsPerSecond", "minorPageFaultsPerSecond"],
            systemUserTime : ["times", "cpuSystemTime", "cpuUserTime"],
            httpConnections : ["times", "clientConnectionsCurrent"],
            totalTime : ["times", "avgQueueTime", "avgRequestTime", "avgIoTime"],
            dataTransfer : ["times", "bytesSentPerSecond", "bytesReceivedPerSecond"],
            requests : ["times", "getsPerSecond", "putsPerSecond", "postsPerSecond",
                "deletesPerSecond", "patchesPerSecond", "headsPerSecond",
                "optionsPerSecond", "othersPerSecond"],
            requestsAsync : ["times", "asyncsPerSecond"]
        },

        //colors for dygraphs
        colors: ["#617e2b", "#296e9c", "#81ccd8", "#7ca530", "#3c3c3c",
            "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"],

        //figure dependend options
        figureDependedOptions: {
            numberOfThreads: {
                header: "Number of Threads"
            },
            residentSize: {
                header: "Resident Size",
                axes: {
                    y: {
                        labelsKMG2: false,
                        axisLabelFormatter: function (y) {
                            return y.toPrecision(2) + "%";
                        },
                        valueFormatter: function (y) {
                            return y.toPrecision(2) + "%";
                        }
                    }
                }
            },
            virtualSize: {
                header: "Virtual Size"
            },
            pageFaults: {
                header : "Page Faults",
                visibility: [true, false],
                labels: ["datetime", "Major Page Faults", "Minor Page Faults"],
                div: "pageFaultsChart",
                labelsKMG2: false
            },
            systemUserTime: {
                div: "systemUserTimeChart",
                header: "System and User Time",
                labels: ["datetime", "System Time", "User Time"],
                stackedGraph: true,
                labelsKMG2: false,
                axes: {
                    y: {
                        valueFormatter: function (y) {
                            return y.toPrecision(3);
                        },
                        axisLabelFormatter: function (y) {
                            if (y === 0) {
                                return 0;
                            }
                            return y.toPrecision(3);
                        }
                    }
                }
            },
            httpConnections: {
                header: "Client Connections"
            },
            totalTime: {
                div: "totalTimeChart",
                header: "Total Time",
                labels: ["datetime", "Queue Time", "Request Time", "IO Time"],
                labelsKMG2: false,
                axes: {
                    y: {
                        valueFormatter: function (y) {
                            return y.toPrecision(3);
                        },
                        axisLabelFormatter: function (y) {
                            if (y === 0) {
                                return 0;
                            }
                            return y.toPrecision(3);
                        }
                    }
                },
                stackedGraph: true
            },
            dataTransfer: {
                header: "Data Transfer",
                labels: ["datetime", "Bytes sent", "Bytes received"],
                stackedGraph: true,
                div: "dataTransferChart"
            },
            requests: {
                header: "Requests",
                labels: ["datetime", "GET", "PUT", "POST", "DELETE", "PATCH", "HEAD", "OPTIONS", "OTHER"],
                stackedGraph: true,
                div: "requestsChart"
            },
            requestsAsync: {
                header: "Async Requests"
            }
        },

        getDashBoardFigures : function (all) {
            var result = [], self = this;
            Object.keys(this.figureDependedOptions).forEach(function (k) {
                if (self.figureDependedOptions[k].div || all) {
                    result.push(k);
                }
            });
            return result;
        },


        //configuration for chart overview
        getDefaultConfig: function (figure) {
            var self = this;
            var result = {
                digitsAfterDecimal: 2,
                drawGapPoints: true,
                fillGraph: true,
                showLabelsOnHighlight: false,
                strokeWidth: 2,
                strokeBorderWidth: 1,
                includeZero: true,
                highlightCircleSize: 0,
                labelsSeparateLines : true,
                strokeBorderColor: '#ffffff',
                interactionModel: {},
                maxNumberWidth : 10,
                colors: [this.colors[0]],
                xAxisLabelWidth: "60",
                rightGap: 10,
                showRangeSelector: false,
                rangeSelectorHeight: 40,
                rangeSelectorPlotStrokeColor: '#365300',
                rangeSelectorPlotFillColor: '#414a4c',
                pixelsPerLabel: 60,
                labelsKMG2: true,
                dateWindow: [
                    new Date().getTime() -
                    this.defaultFrame,
                    new Date().getTime()
                ],
                axes: {
                    x: {
                        valueFormatter: function (d) {
                            return self.xAxisFormat(d);
                        }
                    },
                    y: {
                        ticker: Dygraph.numericLinearTicks
                    }
                }
            };
            if (this.figureDependedOptions[figure]) {
                result = this.mergeObjects(
                    result, this.figureDependedOptions[figure], ["axes"]
                );
                if (result.div && result.labels) {
                    result.colors = this.getColors(result.labels);
                    result.labelsDiv = document.getElementById(result.div + "Legend");
                    result.legend = "always";
                    result.showLabelsOnHighlight = true;
                }
            }
            return result;

        },

        getDetailChartConfig: function (figure) {
            var result = _.extend(
                this.getDefaultConfig(figure),
                {
                    showRangeSelector: true,
                    interactionModel: null,
                    showLabelsOnHighlight: true,
                    highlightCircleSize: 3
                }
            );
            if (figure === "pageFaults") {
                result.visibility = [true, true];
            }
            if (!result.labels) {
                result.labels = ["datetime", result.header];
                result.colors = this.getColors(result.labels);
            }
            return result;
        },

        getColors: function (labels) {
            var colorList;
            colorList = this.colors.concat([]);
            return colorList.slice(0, labels.length - 1);
        }
    };
}());
