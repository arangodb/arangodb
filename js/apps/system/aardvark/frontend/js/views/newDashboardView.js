/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function () {
    "use strict";

    window.newDashboardView = Backbone.View.extend({
        el: '#content',
        interval: 15000, // in milliseconds
        defaultFrame: 20 * 60 * 1000,
        defaultDetailFrame: 14 * 24 * 60 * 60 * 1000,
        history: {},
        graphs: {},
        drawedLegend : false,
        alreadyCalledDetailChart: [],

        events: {
            "click .dashboard-chart": "showDetail",
            "mousedown .dygraph-rangesel-zoomhandle": "stopUpdating",
            "mouseup .dygraph-rangesel-zoomhandle": "startUpdating",
            "mouseleave .dygraph-rangesel-zoomhandle": "startUpdating",
            "click #backToCluster": "returnToClusterView"
        },

        tendencies: {

            asyncRequestsCurrent: ["asyncRequestsCurrent", "asyncRequestsCurrentPercentChange"],
            asyncRequestsAverage: ["asyncPerSecond15M", "asyncPerSecondPercentChange15M"],
            clientConnectionsCurrent: ["clientConnectionsCurrent",
                "clientConnectionsCurrentPercentChange"
            ],
            clientConnectionsAverage: ["clientConnections15M", "clientConnectionsPercentChange15M"],
            numberOfThreadsCurrent: ["numberOfThreadsCurrent", "numberOfThreadsCurrentPercentChange"],
            numberOfThreadsAverage: ["numberOfThreads15M", "numberOfThreadsPercentChange15M"],
            virtualSizeCurrent: ["virtualSizeCurrent", "virtualSizePercentChange"],
            virtualSizeAverage: ["virtualSize15M", "virtualSizePercentChange15M"]
        },

        barCharts : {
            totalTimeDistribution  : ["queueTimeDistributionPercent", "requestTimeDistributionPercent"],
            dataTransferDistribution : ["bytesReceivedDistributionPercent", "bytesSentDistributionPercent"]
        },


        barChartsElementNames : {
            queueTimeDistributionPercent : "Queue Time",
            requestTimeDistributionPercent : "Request Time",
            bytesReceivedDistributionPercent : "Bytes received",
            bytesSentDistributionPercent : "Bytes sent"
        },

        showDetail: function (e) {
            var self = this;
            var figure = $(e.currentTarget).attr("id").replace(/ChartContainer/g, "");
            this.getStatistics(figure);
            var options = this.dygraphConfig.getDetailChartConfig(figure);

            this.detailGraphFigure = figure;
            window.modalView.show(
                "modalGraph.ejs",
                options.header,
                [window.modalView.createCloseButton(
                    this.hidden.bind(this)
                )
                ]
            );

            $('#modal-dialog').toggleClass("modalChartDetail", true);
            $('#modal-body').toggleClass("modal-body", true);

            var dimensions = self.getCurrentSize("#lineChartDetail");
            options.height = dimensions.height;
            options.width = dimensions.width;

            self.detailGraph = new Dygraph(
                document.getElementById("lineChartDetail"),
                self.history[figure],
                options
            );

        },

        hidden: function () {
            this.detailGraph.destroy();
            delete this.detailGraph;
            delete this.detailGraphFigure;
        },


        getCurrentSize: function (div) {
            var height, width;
            height = $(div).height();
            width = $(div).width();
            return {
                height: height,
                width: width
            };
        },

        prepareDygraphs: function () {
            var self = this, options;
            this.dygraphConfig.getDashBoardFigures().forEach(function (f) {
                options = self.dygraphConfig.getDefaultConfig(f);
                var dimensions = self.getCurrentSize(options.div);
                options.height = dimensions.height;
                options.width = dimensions.width;
                self.graphs[f] = new Dygraph(
                    document.getElementById(options.div),
                    self.history[f],
                    options
                );
            });
        },

        initialize: function () {
            this.dygraphConfig = this.options.dygraphConfig;

        },

        updateCharts: function () {
            var self = this;
            if (this.detailGraph) {
                this.updateLineChart(this.detailGraphFigure, true);
                return;
            }
            this.prepareD3Charts();
            this.updateTendencies();
            Object.keys(this.graphs).forEach(function (f) {
                self.updateLineChart(f);
            });
        },

        updateTendencies: function () {
            var self = this, map = this.tendencies;

            Object.keys(map).forEach(function (a) {
                $("#" + a).text(self.history[a][0] + " (" + self.history[a][1] + " %)");
            });
        },


        updateDateWindow: function (graph, isDetailChart) {
            var t = new Date().getTime();
            var borderLeft, borderRight;
            /*    if (isDetailChart) {
             } else {
             }
             */
            return [new Date().getTime() - this.defaultFrame, new Date().getTime()];


        },

        updateLineChart: function (figure, isDetailChart) {

            var opts = {
                file: this.history[figure],
                dateWindow: this.updateDateWindow(isDetailChart)
            };
            if (isDetailChart) {
                this.detailGraph.updateOptions(opts);
            } else {
                this.graphs[figure].updateOptions(opts);
            }
        },

        mergeDygraphHistory: function (newData, i) {
            var self = this, valueList;
            this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {
                if (!self.history[f]) {
                    self.history[f] = [];
                }
                valueList = [];
                self.dygraphConfig.mapStatToFigure[f].forEach(function (a) {
                    if (!newData[a]) {
                        return;
                    }
                    if (a === "times") {
                        valueList.push(new Date(newData[a][i] * 1000));
                    } else {
                        valueList.push(newData[a][i]);
                    }
                });
                if (valueList.length > 1) {
                    self.history[f].push(valueList);
                }
            });
        },

        mergeHistory: function (newData) {
            var self = this, i;
            for (i = 0; i < newData.times.length; ++i) {
                this.mergeDygraphHistory(newData, i);
            }
            Object.keys(this.tendencies).forEach(function (a) {
                if (a === "virtualSizeCurrent" ||
                    a === "virtualSizeAverage") {
                    newData[self.tendencies[a][0]] =
                        newData[self.tendencies[a][0]] / (1024 * 1024 * 1024);
                }
                self.history[a] = [
                    Math.round(newData[self.tendencies[a][0]] * 1000) / 1000,
                    Math.round(newData[self.tendencies[a][1]] * 1000) / 1000
                ];
            });

            Object.keys(this.barCharts).forEach(function (a) {
                /*if (a === "virtualSizeCurrent" ||
                    a === "virtualSizeAverage") {
                    newData[self.tendencies[a][0]] =
                        newData[self.tendencies[a][0]] / (1024 * 1024 * 1024);
                }*/


                self.history[a] = self.mergeBarChartData(self.barCharts[a], newData);
            });
            this.nextStart = newData.nextStart;
        },

        mergeBarChartData : function(attribList, newData) {
            var i, v1 = {
                "key": this.barChartsElementNames[attribList[0]],
                "color": this.dygraphConfig.colors[0],
                "values": []
            }, v2 = {
                "key": this.barChartsElementNames[attribList[1]],
                "color": this.dygraphConfig.colors[1],
                "values": []
            };
            for (i = 0; i < newData[attribList[0]].cuts.length; ++i) {
                v1.values.push({
                    label : this.getLabel(newData[attribList[0]].cuts, i),
                    value : newData[attribList[0]].values[i]
                });
                v2.values.push({
                    label : this.getLabel(newData[attribList[1]].cuts, i),
                    value : newData[attribList[1]].values[i]
                });
            }
            return [v1, v2];
        },

        getLabel : function (cuts, counter) {
            return counter === 0 ? "0 - " + cuts[counter] : cuts[counter -1] + " - " + cuts[counter]
        },

        getStatistics: function (figure) {
            var url = "statistics/full?start=", self = this, dateString;
            if (!figure && this.nextStart) {
                url += this.nextStart;
            } else if (!figure && !this.nextStart) {
                url += (new Date().getTime() - this.defaultFrame) / 1000;
            } else {
                if (this.alreadyCalledDetailChart.indexOf(figure) !== -1) {
                    return;
                }
                this.history[figure] = [];
                url += (new Date().getTime() - this.defaultDetailFrame) / 1000;
                url += "&filter=" + this.dygraphConfig.mapStatToFigure[figure].join();
                this.alreadyCalledDetailChart.push(figure);
            }
            console.log(url);
            $.ajax(
                url,
                {async: false}
            ).done(
                function (d) {
                    console.log("got result", d);
                    self.mergeHistory(d);
                }
            );

        },

        prepareD3Charts: function () {
            //distribution bar charts
            var v, self = this, barCharts = {
                totalTimeDistribution : ["queueTimeDistributionPercent", "requestTimeDistributionPercent"],
                dataTransferDistribution : ["bytesReceivedDistributionPercent", "bytesSentDistributionPercent"]
            };

            _.each(Object.keys(barCharts), function (k) {
                nv.addGraph(function () {
                    console.log('#' + k + ' svg');
                    var chart = nv.models.multiBarHorizontalChart()
                        .x(function (d) {
                            return d.label;
                        })
                        .y(function (d) {
                            return d.value;
                        })
                        //.margin({top: 30, right: 20, bottom: 50, left: 175})
                        .margin({left: 80})
                        .showValues(true)
                        .showYAxis(true)
                        .showXAxis(true)
                        .transitionDuration(350)
                        .tooltips(false)
                        .showLegend(true)
                        .showControls(false);

                    /*chart.yAxis
                        .tickFormat(d3.format(',.2f'));*//**//**/
                    chart.xAxis.showMaxMin(false);
                    chart.yAxis.showMaxMin(false);
                    /*chart.xAxis
                     .tickFormat(d3.format(',.2f'));*/
                    d3.select('#' + k + 'Container svg')
                        .datum(self.history[k])
                        .call(chart)
                        .append("text")
                        .attr("x", 0)
                        .attr("y", 16)
                        .style("font-size", "20px")
                        .style("font-weight", 400)
                        .style("font-family", "Open Sans")
                        .text("Distribution");

                    nv.utils.windowResize(chart.update);
                    if (!self.drawedLegend) {
                        var v1 = self.history[k][0]["key"];
                        var v2 = self.history[k][1]["key"];
                        $('#' + k + "Legend").append(
                            '<span style="font-weight: bold; color: ' + self.history[k][0].color + ';">' +
                            '<div style="display: inline-block; position: relative; bottom: .5ex; padding-left: 1em;' +
                            ' height: 1px; border-bottom: 2px solid ' + self.history[k][0].color + ';"></div>' +
                                + " " + v1 + '</span><br>' +
                            '<span style="font-weight: bold; color: ' + self.history[k][1].color + ';">' +
                            '<div style="display: inline-block; position: relative; bottom: .5ex; padding-left: 1em;' +
                            ' height: 1px; border-bottom: 2px solid ' + self.history[k][1].color + ';"></div>' +
                            + " " + v2 + '</span><br>'
                        );
                    }
                });
            });
            this.drawedLegend = true;

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
            self.timer = window.setInterval(function () {
                    self.getStatistics();
                    self.updateCharts();
                },
                self.interval
            );
        },


        resize: function () {
            var self = this, dimensions;
            Object.keys(this.graphs).forEach(function (f) {
                var g = self.graphs[f];
                dimensions = self.getCurrentSize(g.maindiv_.id);
                g.resize(dimensions.width, dimensions.height);
            });
        },

        template: templateEngine.createTemplate("newDashboardView.ejs"),

        render: function () {
            $(this.el).html(this.template.render());
            this.getStatistics();
            this.prepareDygraphs();
            this.prepareD3Charts();
            this.updateTendencies();
            this.startUpdating();
        },


        returnToClusterView: function () {
            window.history.back();
        }
    });
}());
