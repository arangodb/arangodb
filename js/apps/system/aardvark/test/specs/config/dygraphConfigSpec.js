/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, jasmine, beforeEach, afterEach, Backbone, it, spyOn, expect*/
/*global $*/

(function () {
    "use strict";

    describe("dashBoardConfig", function () {

        var col;

        beforeEach(function () {
            col = window.dygraphConfig;
        });

        it("zeropad", function () {
            expect(col.defaultFrame).toEqual(20 * 60 * 1000);
            expect(col.zeropad(9)).toEqual("09");
            expect(col.zeropad(11)).toEqual(11);
        });

        it("xAxisFormat", function () {
            expect(col.xAxisFormat(-1)).toEqual("");
             var d = new Date();
            expect(col.xAxisFormat(d.getTime())).toEqual(col.zeropad(d.getHours()) + ":"
                + col.zeropad(d.getMinutes()) + ":"
                + col.zeropad(d.getSeconds()));
        });

        it("mergeObjects", function () {
            var o1 = {
                a : 1,
                b : {
                    x : 2
                    }
                },
           o2 = {
                a : 2,
                c : {
                    x : 2
                }
            };


            expect(col.mergeObjects(o1, o2, ["b", "c"])).toEqual(
                {
                    a : 2,
                    b : {
                        x : 2
                    },
                    c : {
                        x : 2
                    }
                }

            );
        });

        it("mergeObjects with empty attrib list", function () {
            var o1 = {
                    a : 1,
                    b : {
                        x : 2
                    }
                },
                o2 = {
                    a : 2,
                    b : {
                        x : 2
                    }
                };


            expect(col.mergeObjects(o1, o2)).toEqual(
                {
                    a : 2,
                    b : {
                        x : 2
                    }
                }

            );
        });

        it("check constants", function () {
            expect(col.mapStatToFigure).toEqual(
                {
                    numberOfThreads : ["times", "numberOfThreads"],
                    residentSize : ["times", "residentSizePercent"],
                    virtualSize : ["times", "virtualSize"],
                    pageFaults : ["times", "majorPageFaultsPerSecond", "minorPageFaultsPerSecond"],
                    systemUserTime : ["times", "systemTimePerSecond", "userTimePerSecond"],
                    httpConnections : ["times", "clientConnections"],
                    totalTime : ["times", "avgQueueTime", "avgRequestTime", "avgIoTime"],
                    dataTransfer : ["times", "bytesSentPerSecond", "bytesReceivedPerSecond"],
                    requests : ["times", "getsPerSecond", "putsPerSecond", "postsPerSecond",
                        "deletesPerSecond", "patchesPerSecond", "headsPerSecond",
                        "optionsPerSecond", "othersPerSecond"],
                    requestsAsync : ["times", "asyncPerSecond"]
                }
            );

            expect(col.colors).toEqual(
                ["#617e2b", "#296e9c", "#81ccd8", "#7ca530", "#3c3c3c",
                    "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"]
            );

            expect(col.figureDependedOptions).toEqual(
                {
                    clusterAverageRequestTime : {
                        showLabelsOnHighlight : true,
                        title : 'Average request time in milliseconds',
                        axes: {
                            y: {
                                valueFormatter: jasmine.any(Function),
                                axisLabelFormatter: jasmine.any(Function)
                            }
                        }
                    },

                    numberOfThreads: {
                        header: "Number of Threads"
                    },
                    residentSize: {
                        header: "Resident Size",
                        axes: {
                            y: {
                                labelsKMG2: false,
                                axisLabelFormatter: jasmine.any(Function),
                                valueFormatter: jasmine.any(Function)
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
                                valueFormatter: jasmine.any(Function),
                                axisLabelFormatter: jasmine.any(Function)
                            }
                        }
                    },
                    httpConnections: {
                        header: "Client Connections"
                    },
                    totalTime: {
                        div: "totalTimeChart",
                        header: "Total Time",
                        labels: [ 'datetime', 'Queue', 'Computation', 'I/O' ],
                        labelsKMG2: false,
                        axes: {
                            y: {
                                valueFormatter: jasmine.any(Function),
                                axisLabelFormatter: jasmine.any(Function)
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
                        labels: ["datetime", "GET", "PUT", "POST", "DELETE",
                            "PATCH", "HEAD", "OPTIONS", "OTHER"],
                        stackedGraph: true,
                        div: "requestsChart"
                    },
                    requestsAsync: {
                        header: "Async Requests"
                    }
                }
            );
            expect(col.figureDependedOptions.clusterAverageRequestTime.axes.y.
                axisLabelFormatter(0)).toEqual(0);
            expect(col.figureDependedOptions.clusterAverageRequestTime.axes.y.
                axisLabelFormatter(3.4567)).toEqual(3.46);
            expect(col.figureDependedOptions.clusterAverageRequestTime.axes.y.
                valueFormatter(3.4567)).toEqual(3.46);

            expect(col.figureDependedOptions.systemUserTime.axes.y.
                axisLabelFormatter(0)).toEqual(0);
            expect(col.figureDependedOptions.systemUserTime.axes.y.
                axisLabelFormatter(3.4567)).toEqual(3.46);
            expect(col.figureDependedOptions.systemUserTime.axes.y.
                valueFormatter(3.4567)).toEqual(3.46);

            expect(col.figureDependedOptions.totalTime.axes.y.
                axisLabelFormatter(0)).toEqual(0);
            expect(col.figureDependedOptions.totalTime.axes.y.
                axisLabelFormatter(3.4567)).toEqual(3.46);
            expect(col.figureDependedOptions.totalTime.axes.y.
                valueFormatter(3.4567)).toEqual(3.46);

            expect(col.figureDependedOptions.residentSize.axes.y.
                axisLabelFormatter(3.4567)).toEqual("346%");
            expect(col.figureDependedOptions.residentSize.axes.y.
                valueFormatter(3.4567)).toEqual("346%");

        });

        it("getDashBoardFigures", function () {
            var result = [], self = this;
            Object.keys(col.figureDependedOptions).forEach(function (k) {
                if (col.figureDependedOptions[k].div) {
                    result.push(k);
                }
            });
            expect(col.getDashBoardFigures()).toEqual(result);
        });

        it("get all DashBoardFigures", function () {
            var result = [], self = this;
            Object.keys(col.figureDependedOptions).forEach(function (k) {
                result.push(k);
            });
            expect(col.getDashBoardFigures(true)).toEqual(result);
        });

        it("getDefaultConfig", function () {
            var res = col.getDefaultConfig("systemUserTime");
            expect(res.digitsAfterDecimal).toEqual(1);
            expect(res.drawGapPoints).toEqual(true);
            expect(res.fillGraph).toEqual(true);
            expect(res.showLabelsOnHighlight).toEqual(true);
            expect(res.strokeWidth).toEqual(2);
            expect(res.strokeBorderWidth).toEqual(0.5);
            expect(res.includeZero).toEqual(true);
            expect(res.highlightCircleSize).toEqual(0);
            expect(res.labelsSeparateLines ).toEqual(true);
            expect(res.strokeBorderColor).toEqual('#ffffff');
            expect(res.interactionModel).toEqual({});
            expect(res.maxNumberWidth ).toEqual(10);
            expect(res.colors).toEqual([ '#617e2b', '#296e9c' ]);
            expect(res.xAxisLabelWidth).toEqual("50");
            expect(res.rightGap).toEqual(15);
            expect(res.showRangeSelector).toEqual(false);
            expect(res.rangeSelectorHeight).toEqual(50);
            expect(res.rangeSelectorPlotStrokeColor).toEqual('#365300');
            //expect(res.rangeSelectorPlotFillColor).toEqual('#414a4c');
            expect(res.pixelsPerLabel).toEqual(50);
            expect(res.labelsKMG2 ).toEqual(false);
            expect(res.axes).toEqual({
                x : {
                    valueFormatter : jasmine.any(Function)
                },
                y : {
                    valueFormatter : jasmine.any(Function),
                    axisLabelFormatter: jasmine.any(Function)
                }
            });
            expect(res.div).toEqual("systemUserTimeChart");
            expect(res.header).toEqual("System and User Time");
            expect(res.labels).toEqual(["datetime",
                "System Time", "User Time"]);
            expect(res.labelsDiv ).toEqual(null);
            expect(res.legend ).toEqual("always");
            expect(res.axes.x.valueFormatter(-1)).toEqual(
                col.xAxisFormat(-1));
        });


        it("getDetailConfig", function () {
            var res = col.getDetailChartConfig("pageFaults");
            expect(res.digitsAfterDecimal).toEqual(1);
            expect(res.drawGapPoints).toEqual(true);
            expect(res.fillGraph).toEqual(true);
            expect(res.showLabelsOnHighlight).toEqual(true);
            expect(res.strokeWidth).toEqual(2);
            expect(res.strokeBorderWidth).toEqual(0.5);
            expect(res.includeZero).toEqual(true);
            expect(res.highlightCircleSize).toEqual(3);
            expect(res.labelsSeparateLines ).toEqual(true);
            expect(res.strokeBorderColor).toEqual('#ffffff');
            expect(res.interactionModel).toEqual(null);
            expect(res.maxNumberWidth ).toEqual(10);
            expect(res.colors).toEqual([ '#617e2b', '#296e9c' ]);
            expect(res.xAxisLabelWidth).toEqual("50");
            expect(res.rightGap).toEqual(15);
            expect(res.showRangeSelector).toEqual(true);
            expect(res.rangeSelectorHeight).toEqual(50);
            expect(res.rangeSelectorPlotStrokeColor).toEqual('#365300');
            //expect(res.rangeSelectorPlotFillColor).toEqual('#414a4c');
            expect(res.pixelsPerLabel).toEqual(50);
            expect(res.labelsKMG2 ).toEqual(false);
            expect(res.div).toEqual("pageFaultsChart");
            expect(res.header).toEqual("Page Faults");
            expect(res.labels).toEqual(["datetime", "Major Page Faults",
                "Minor Page Faults"]);
            expect(res.legend ).toEqual("always");
            expect(res.visibility).toEqual([true, true]);
        });

        it("getDetailConfig with empty labels", function () {
            var res = col.getDetailChartConfig("requestsAsync");
            expect(res.digitsAfterDecimal).toEqual(1);
            expect(res.drawGapPoints).toEqual(true);
            expect(res.fillGraph).toEqual(true);
            expect(res.showLabelsOnHighlight).toEqual(true);
            expect(res.strokeWidth).toEqual(2);
            expect(res.strokeBorderWidth).toEqual(0.5);
            expect(res.includeZero).toEqual(true);
            expect(res.highlightCircleSize).toEqual(3);
            expect(res.labelsSeparateLines ).toEqual(true);
            expect(res.strokeBorderColor).toEqual('#ffffff');
            expect(res.interactionModel).toEqual(null);
            expect(res.maxNumberWidth ).toEqual(10);
            expect(res.colors).toEqual([ '#617e2b' ]);
            expect(res.xAxisLabelWidth).toEqual("50");
            expect(res.rightGap).toEqual(15);
            expect(res.showRangeSelector).toEqual(true);
            expect(res.rangeSelectorHeight).toEqual(50);
            expect(res.rangeSelectorPlotStrokeColor).toEqual('#365300');
            //expect(res.rangeSelectorPlotFillColor).toEqual('#414a4c');
            expect(res.pixelsPerLabel).toEqual(50);
            expect(res.labelsKMG2 ).toEqual(true);
            expect(res.legend ).toEqual("always");
        });

    });

}());

