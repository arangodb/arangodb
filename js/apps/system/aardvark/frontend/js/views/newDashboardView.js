/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function () {
    "use strict";

    window.newDashboardView = Backbone.View.extend({
        el: '#content',
        interval: 10000, // in milliseconds
        defaultFrame: 20 * 60 * 1000,
        defaultDetailFrame: 2 * 24 * 60 * 60 * 1000,
        history: {},
        graphs: {},
        alreadyCalledDetailChart: [],

        events: {
            "click .dashboard-chart": "showDetail",
            "click #backToCluster": "returnToClusterView"
        },

        tendencies: {

            asyncRequestsCurrent: ["asyncRequestsCurrent", "asyncRequestsCurrentPercentChange"],
            asyncRequestsAverage: ["asyncPerSecond15M", "asyncPerSecondPercentChange15M"],
            clientConnectionsCurrent: ["clientConnectionsCurrent",
                "clientConnectionsCurrentPercentChange"
            ],
            clientConnectionsAverage: [
                "clientConnections15M", "clientConnectionsPercentChange15M"
            ],
            numberOfThreadsCurrent: [
                "numberOfThreadsCurrent", "numberOfThreadsCurrentPercentChange"],
            numberOfThreadsAverage: ["numberOfThreads15M", "numberOfThreadsPercentChange15M"],
            virtualSizeCurrent: ["virtualSizeCurrent", "virtualSizePercentChange"],
            virtualSizeAverage: ["virtualSize15M", "virtualSizePercentChange15M"]
        },

        barCharts: {
            totalTimeDistribution: [
                "queueTimeDistributionPercent", "requestTimeDistributionPercent"
            ],
            dataTransferDistribution: [
                "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"]
        },


        barChartsElementNames: {
            queueTimeDistributionPercent: "Queue Time",
            requestTimeDistributionPercent: "Request Time",
            bytesSentDistributionPercent: "Bytes sent",
            bytesReceivedDistributionPercent: "Bytes received"

        },


        getDetailFigure : function (e) {
            var figure = $(e.currentTarget).attr("id").replace(/ChartContainer/g, "");
            figure = figure.replace(/DistributionContainer/g, "");
            figure = figure.replace(/Container/g, "");
            if (figure === "asyncRequests") {
                figure = "requestsAsync";
            } else if (figure === "clientConnections") {
                figure = "httpConnections";
            }
            return figure;
        },

        showDetail: function (e) {
            var self = this,
            figure = this.getDetailFigure(e), options;
            this.getStatistics(figure);
            this.detailGraphFigure = figure;
            options = this.dygraphConfig.getDetailChartConfig(figure);
            window.modalView.hideFooter = true;
            window.modalView.hide();
            window.modalView.show(
                "modalGraph.ejs",
                options.header,
                undefined,
                undefined,
                undefined,
                this.events

            );

            options = this.dygraphConfig.getDetailChartConfig(figure);
            window.modalView.hideFooter = false;

            $('#modal-dialog').on('hidden', function () {
                self.hidden();
            });
            //$('.modal-body').css({"max-height": "100%" });
            $('#modal-dialog').toggleClass("modal-chart-detail", true);
            options.height = $('.modal-chart-detail').height() * 0.7;
            options.width = $('.modal-chart-detail').width() * 0.84;
            this.detailGraph = new Dygraph(
                document.getElementById("lineChartDetail"),
                this.history[figure],
                options
            );
        },

        hidden: function () {
            this.detailGraph.destroy();
            delete this.detailGraph;
            delete this.detailGraphFigure;
        },


        getCurrentSize: function (div) {
            if (div.substr(0,1) !== "#") {
                div = "#" + div;
            }
            var height, width;
            $(div).attr("style", "");
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
            this.server = this.options.serverToShow;
            this.events["click .dashboard-chart"] = this.showDetail.bind(this);
            this.events["mousedown .dygraph-rangesel-zoomhandle"] = this.stopUpdating.bind(this);
            this.events["mouseup .dygraph-rangesel-zoomhandle"] = this.startUpdating.bind(this);
        },

        updateCharts: function () {
            var self = this;
            if (this.detailGraph) {
                this.updateLineChart(this.detailGraphFigure, true);
                return;
            }
            this.prepareD3Charts(true);
            this.prepareResidentSize(true);
            this.updateTendencies();
            Object.keys(this.graphs).forEach(function (f) {
                self.updateLineChart(f, false);
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
            if (isDetailChart && graph.dateWindow_) {
                borderLeft = graph.dateWindow_[0];
                borderRight = t - graph.dateWindow_[1] - this.interval * 5 > 0 ?
                        graph.dateWindow_[1] : t;
                return [borderLeft, borderRight];
            }
            return [t - this.defaultFrame, t];


        },

        updateLineChart: function (figure, isDetailChart) {
            var g = isDetailChart ? this.detailGraph : this.graphs[figure],
                opts = {
                file: this.history[figure],
                dateWindow: this.updateDateWindow(g, isDetailChart)
            };
            g.updateOptions(opts);
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

        mergeHistory: function (newData, detailMode) {
            var self = this, i;
            for (i = 0; i < newData.times.length; ++i) {
                this.mergeDygraphHistory(newData, i);
            }
            if (detailMode) {
                return;
            }
            Object.keys(this.tendencies).forEach(function (a) {
                if (a === "virtualSizeCurrent" ||
                    a === "virtualSizeAverage") {
                    newData[self.tendencies[a][0]] =
                        newData[self.tendencies[a][0]] / (1024 * 1024 * 1024);
                }
                self.history[a] = [
                    Math.round(newData[self.tendencies[a][0]] * 1000) / 1000,
                    Math.round(newData[self.tendencies[a][1]] * 1000 * 100) / 1000
                ];
            });

            Object.keys(this.barCharts).forEach(function (a) {
                self.history[a] = self.mergeBarChartData(self.barCharts[a], newData);
            });
            self.history.residentSizeChart =
                [
                    {
                        "key": "",
                        "color": this.dygraphConfig.colors[1],
                        "values": [
                            {
                                label: "used",
                                value: newData.residentSizePercent[
                                    newData.residentSizePercent.length - 1] * 100
                            }
                        ]
                    },
                    {
                        "key": "",
                        "color": this.dygraphConfig.colors[0],
                        "values": [
                            {
                                label: "used",
                                value: 100 - newData.residentSizePercent[
                                    newData.residentSizePercent.length - 1] * 100
                            }
                        ]
                    }

                ]
            ;


            this.nextStart = newData.nextStart;
        },

        mergeBarChartData: function (attribList, newData) {
            var i, v1 = {
                "key": this.barChartsElementNames[attribList[0]],
                "color": this.dygraphConfig.colors[0],
                "values": []
            }, v2 = {
                "key": this.barChartsElementNames[attribList[1]],
                "color": this.dygraphConfig.colors[1],
                "values": []
            };
            for (i = 0; i < newData[attribList[0]].values.length; ++i) {
                v1.values.push({
                    label: this.getLabel(newData[attribList[0]].cuts, i),
                    value: newData[attribList[0]].values[i]
                });
                v2.values.push({
                    label: this.getLabel(newData[attribList[1]].cuts, i),
                    value: newData[attribList[1]].values[i]
                });
            }
            return [v1, v2];
        },

        getLabel: function (cuts, counter) {
            if (!cuts[counter]) {
                return ">" + cuts[counter - 1];
            }
            return counter === 0 ? "0 - " +
                cuts[counter] : cuts[counter - 1] + " - " + cuts[counter];
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
            if (this.server) {
                url += "&serverEndpoint=" + encodeURIComponent(this.server.endpoint) +
                    "&DbServer=" + this.server.target;
            }
            $.ajax(
                url,
                {async: false}
            ).done(
                function (d) {
                    self.mergeHistory(d, !!figure);
                }
            );

        },

        prepareResidentSize: function (update) {
            var dimensions = this.getCurrentSize('#residentSizeChartContainer'),
                self = this, currentP =
                    Math.round(self.history.residentSizeChart[0].values[0].value * 100) /100;
            nv.addGraph(function () {
                var chart = nv.models.multiBarHorizontalChart()
                    /*.width(dimensions.width * 0.3)
                    .height(dimensions.height)*/
                    .x(function (d) {
                        return d.label;
                    })
                    .y(function (d) {
                        return d.value;
                    })
                    .width(dimensions.width * 0.95)
                    .height(dimensions.height)
                    .margin({
                        //top: dimensions.height / 8,
                        right: dimensions.width / 10
                        //bottom: dimensions.height / 22,
                        //left: dimensions.width / 6*/
                    })
                    .showValues(false)
                    .showYAxis(false)
                    .showXAxis(false)
                    .transitionDuration(350)
                    .tooltips(false)
                    .showLegend(false)
                    .stacked(true)
                    .showControls(false);
                chart.yAxis
                    .tickFormat(function (d) {return d + "%";});
                chart.xAxis.showMaxMin(false);
                chart.yAxis.showMaxMin(false);
                d3.select('#residentSizeChart svg')
                    .datum(self.history.residentSizeChart)
                    .call(chart);
                d3.select('#residentSizeChart svg').select('.nv-zeroLine').remove();
                if (update) {
                    d3.select('#residentSizeChart svg').select('#total').remove();
                    d3.select('#residentSizeChart svg').select('#percentage').remove();
                }

                    d3.select('#residentSizeChart svg').selectAll('#total')
                    .data([Math.round(self.history.virtualSizeCurrent[0] * 1000) / 1000 + "GB"])
                    .enter()
                    .append("text")
                    .style("font-size", dimensions.height / 8 + "px")
                    .style("font-weight", 400)
                    .style("font-family", "Open Sans")
                    .attr("id", "total")
                    .attr("x", dimensions.width /1.15)
                    .attr("y", dimensions.height/ 2.1)
                    .text(function(d){return d;});
                    d3.select('#residentSizeChart svg').selectAll('#percentage')
                        .data([Math.round(self.history.virtualSizeCurrent[0] * 1000) / 1000 + "GB"])
                        .enter()
                        .append("text")
                        .style("font-size", dimensions.height / 10 + "px")
                        .style("font-weight", 400)
                        .style("font-family", "Open Sans")
                        .attr("id", "percentage")
                        .attr("x", dimensions.width * 0.1)
                        .attr("y", dimensions.height/ 2.1)
                        .text(currentP + " %");
                nv.utils.windowResize(chart.update);
            });
        },


        prepareD3Charts: function (update) {
            var v, self = this, barCharts = {
                totalTimeDistribution: [
                    "queueTimeDistributionPercent", "requestTimeDistributionPercent"],
                dataTransferDistribution: [
                    "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"]
            }, f;

            _.each(Object.keys(barCharts), function (k) {
                var dimensions = self.getCurrentSize('#' + k
                    + 'Container .dashboard-interior-chart');
                if (dimensions.width > 400 ) {
                    f = 18;
                } else if (dimensions.width > 300) {
                    f = 16;
                } else if (dimensions.width > 200) {
                    f = 14;
                } else if (dimensions.width > 100) {
                    f = 12;
                } else {
                    f = 10;
                }
                nv.addGraph(function () {
                    var chart = nv.models.multiBarHorizontalChart()
                        .x(function (d) {
                            return d.label;
                        })
                        .y(function (d) {
                            return d.value;
                        })
                        .width(dimensions.width)
                        .height(dimensions.height)
                        .margin({
                            top: dimensions.height / 8,
                            right: dimensions.width / 35,
                            bottom: dimensions.height / 22,
                            left: dimensions.width / 6
                        })
                        .showValues(false)
                        .showYAxis(true)
                        .showXAxis(true)
                        .transitionDuration(350)
                        .tooltips(false)
                        .showLegend(false)
                        .showControls(false);

                    chart.yAxis
                     .tickFormat(function (d) {return Math.round(d* 100 * 100) / 100 + "%";});


                    d3.select('#' + k + 'Container svg')
                        .datum(self.history[k])
                        .call(chart);

                    nv.utils.windowResize(chart.update);
                    if (!update) {
                        d3.select('#' + k + 'Container svg')
                            .append("text")
                            .attr("x", dimensions.width * 0.5)
                            .attr("y", dimensions.height / 12)
                            .attr("id", "distributionHead")
                            .style("font-size", f + "px")
                            .style("font-weight", 400)
                            .classed("distributionHeader", true)
                            .style("font-family", "Open Sans")
                            .text("Distribution");
                        var v1 = self.history[k][0].key;
                        var v2 = self.history[k][1].key;
                        $('#' + k + "Legend").append(
                            '<span style="font-weight: bold; color: ' +
                                self.history[k][0].color + ';">' +
                                '<div style="display: inline-block; position: relative;' +
                                ' bottom: .5ex; padding-left: 1em;' +
                                ' height: 1px; border-bottom: 2px solid ' +
                                self.history[k][0].color + ';"></div>'
                                + " " + v1 + '</span><br>' +
                                '<span style="font-weight: bold; color: ' +
                                self.history[k][1].color + ';">' +
                                '<div style="display: inline-block; position: ' +
                                'relative; bottom: .5ex; padding-left: 1em;' +
                                ' height: 1px; border-bottom: 2px solid ' +
                                self.history[k][1].color + ';"></div>'
                                + " " + v2 + '</span><br>'
                        );
                    } else {
                        d3.select('#' + k + 'Container svg').select('.distributionHeader').remove();
                        d3.select('#' + k + 'Container svg')
                            .append("text")
                            .attr("x", dimensions.width * 0.5)
                            .attr("y", dimensions.height / 12)
                            .attr("id", "distributionHead")
                            .style("font-size", f + "px")
                            .style("font-weight", 400)
                            .classed("distributionHeader", true)
                            .style("font-family", "Open Sans")
                            .text("Distribution");
                    }
                });
            });

        },

        stopUpdating: function () {
            this.isUpdating = false;
        },

        startUpdating: function () {
            var self = this;
            if (self.timer) {
                return;
            }
            self.isUpdating = true;
            self.timer = window.setInterval(function () {
                    self.getStatistics();
                    if (self.isUpdating === false) {
                        return;
                    }
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
            this.prepareD3Charts(true);
            this.prepareResidentSize(true);
        },

        template: templateEngine.createTemplate("newDashboardView.ejs"),

        render: function (modalView) {
            if (!modalView)  {
                $(this.el).html(this.template.render());
            }
            this.getStatistics();
            this.prepareDygraphs();
            this.prepareD3Charts();
            this.prepareResidentSize();
            this.updateTendencies();
            this.startUpdating();
        },


        returnToClusterView: function () {
            window.history.back();
        }
    });
}());
