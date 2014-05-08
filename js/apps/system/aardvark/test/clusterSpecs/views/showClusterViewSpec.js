/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
    "use strict";

    describe("The ClusterDownView", function () {

        var view, serverDummy, arangoDocumentsDummy, statisticsDescriptionDummy,
            jqueryDummy,serverDummy2;

        beforeEach(function () {
            window.App = {
                navigate: function () {
                    throw "This should be a spy";
                },
                addAuth : {
                    bind : function () {
                        return "authBinding";
                    }
                }
            };
            arangoDocumentsDummy = {
                getStatisticsHistory : function () {

                },
                history : [
                    {
                        time : 20000,
                        server : {
                            uptime : 10000
                        },
                        client : {
                            totalTime : {
                                count : 0,
                                sum : 0
                            }
                        }
                    },
                    {
                        time : 30000,
                        server : {
                            uptime : 100
                        },
                        client : {
                            totalTime : {
                                count : 1,
                                sum : 4
                            }
                        }
                    }

                ]
            };
            serverDummy = {
                getList : function () {
                },
                byAddress : function () {

                },
                getStatuses : function (a) {
                },

                findWhere : function () {

                },
                forEach : function () {

                }

            };
            serverDummy2 = {
                getList : function () {
                },
                byAddress : function () {

                },
                getStatuses : function (a) {
                },

                findWhere : function () {

                },
                forEach : function () {

                },
                first : function () {

                }

            };


            statisticsDescriptionDummy = {
                fetch : function () {

                }

            };

            spyOn(window, "ClusterServers").andReturn(serverDummy);
            spyOn(window, "ClusterDatabases").andReturn(serverDummy);
            spyOn(window, "ClusterCoordinators").andReturn(serverDummy2);
            spyOn(window, "ClusterCollections").andReturn(serverDummy);
            spyOn(window, "ClusterShards").andReturn(serverDummy);
            spyOn(window, "arangoDocuments").andReturn(arangoDocumentsDummy);
            spyOn(window, "StatisticsDescription").andReturn(statisticsDescriptionDummy);

            spyOn(statisticsDescriptionDummy, "fetch");
            view = new window.ShowClusterView({dygraphConfig : window.dygraphConfig});
            expect(view.interval).toEqual(10000);
            expect(view.isUpdating).toEqual(true);
            expect(view.knownServers).toEqual([]);
            expect(view.graph).toEqual(undefined);

            expect(view.graphShowAll).toEqual(false);
            expect(window.ClusterServers).toHaveBeenCalledWith([], {
                interval: view.interval
            });
            expect(window.ClusterCoordinators).toHaveBeenCalledWith([], {
                interval: view.interval
            });
            expect(window.ClusterDatabases).toHaveBeenCalledWith([], {
                interval: view.interval
            });
            expect(statisticsDescriptionDummy.fetch).toHaveBeenCalledWith({
                async: false
            });
        });

        afterEach(function () {
            delete window.App;
        });

        it("assert the basics", function () {


            expect(view.events).toEqual({
                "change #selectDB"        : "updateCollections",
                "change #selectCol"       : "updateShards",
                "click .dbserver"         : "dashboard",
                "click .coordinator"      : "dashboard",
                "click #lineGraph"        : "showDetail"
            });
            expect(view.defaultFrame).toEqual(20 * 60 * 1000);
        });

        it("assert replaceSVGs", function () {
            spyOn($.fn, "each").andCallFake(function (a) {
                a();
            });
            spyOn($, "get").andCallFake(function (a, b, c) {
                b();
            });

            spyOn($.fn, "find").andCallThrough();
            view.replaceSVGs();
            expect($.fn.find).toHaveBeenCalledWith("svg");

        });

        it("assert updateServerTime", function () {
            view.serverTime = 10;
            var before = view.serverTime;
            view.updateServerTime();
            expect(view.serverTime > before).toEqual(true);

        });


        it("assert setShowAll", function () {
            view.setShowAll();
            expect(view.graphShowAll).toEqual(true);

        });


        it("assert resetShowAll", function () {
            spyOn(view, "renderLineChart");
            view.resetShowAll();
            expect(view.graphShowAll).toEqual(false);
            expect(view.renderLineChart).toHaveBeenCalled();

        });

        it("assert listByAddress", function () {
            spyOn(serverDummy2, "byAddress").andReturn("byAddress");
            spyOn(serverDummy, "byAddress").andReturn("byAddress");
            view.listByAddress();
            expect(view.graphShowAll).toEqual(false);
            expect(serverDummy.byAddress).toHaveBeenCalledWith();
            expect(serverDummy2.byAddress).toHaveBeenCalledWith("byAddress");

        });


        it("assert updateCollections", function () {

            jqueryDummy = {
                find : function () {
                   return jqueryDummy;
                },
                attr : function () {
                },
                html : function () {
                    return jqueryDummy;
                },
                append : function () {
                    return jqueryDummy;
                }


            };

            spyOn(window, "$").andReturn(jqueryDummy);
            spyOn(jqueryDummy, "find").andCallThrough();
            spyOn(jqueryDummy, "attr").andReturn("dbName");
            spyOn(jqueryDummy, "html");
            spyOn(jqueryDummy, "append");

            spyOn(serverDummy, "getList").andReturn(
                [
                    {name : "a"},
                    {name : "b"},
                    {name : "c"}
                ]

            );

            spyOn(view, "updateShards");

            view.updateCollections();

            expect(window.$).toHaveBeenCalledWith("#selectDB");
            expect(window.$).toHaveBeenCalledWith("#selectCol");

            expect(view.updateShards).toHaveBeenCalled();

            expect(serverDummy.getList).toHaveBeenCalledWith("dbName");

            expect(jqueryDummy.find).toHaveBeenCalledWith(":selected");
            expect(jqueryDummy.attr).toHaveBeenCalledWith("id");
            expect(jqueryDummy.html).toHaveBeenCalledWith("");
            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "c" + "\">" + "c" + "</option>"
            );
            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "a" + "\">" + "a" + "</option>"
            );

            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "b" + "\">" + "b" + "</option>"
            );


        });

        it("assert updateShards", function () {

            jqueryDummy = {
                find : function () {
                    return jqueryDummy;
                },
                attr : function () {
                },
                html : function () {
                    return jqueryDummy;
                },
                append : function () {
                    return jqueryDummy;
                }


            };

            spyOn(window, "$").andReturn(jqueryDummy);
            spyOn(jqueryDummy, "find").andCallThrough();
            spyOn(jqueryDummy, "attr").andReturn("dbName");
            spyOn(jqueryDummy, "html");
            spyOn(jqueryDummy, "append");

            spyOn(serverDummy, "getList").andReturn(
                [
                    {server : "a", shards : {length : 1}},
                    {server : "b", shards : {length : 2}},
                    {server : "c", shards : {length : 3}}
                ]

            );

            spyOn(serverDummy2, "getList").andReturn(
                [
                    {server : "a", shards : {length : 1}},
                    {server : "b", shards : {length : 2}},
                    {server : "c", shards : {length : 3}}
                ]

            );


            view.updateShards();

            expect(window.$).toHaveBeenCalledWith("#selectDB");
            expect(window.$).toHaveBeenCalledWith("#selectCol");
            expect(window.$).toHaveBeenCalledWith(".shardCounter");
            expect(window.$).toHaveBeenCalledWith("#aShards");
            expect(window.$).toHaveBeenCalledWith("#bShards");
            expect(window.$).toHaveBeenCalledWith("#cShards");


            expect(serverDummy.getList).toHaveBeenCalledWith("dbName", "dbName");

            expect(jqueryDummy.find).toHaveBeenCalledWith(":selected");
            expect(jqueryDummy.attr).toHaveBeenCalledWith("id");
            expect(jqueryDummy.html).toHaveBeenCalledWith("0");
            expect(jqueryDummy.html).toHaveBeenCalledWith(1);
            expect(jqueryDummy.html).toHaveBeenCalledWith(2);
            expect(jqueryDummy.html).toHaveBeenCalledWith(3);


        });


        it("assert updateServerStatus", function () {
            jqueryDummy = {
                attr : function () {
                }
            };
            spyOn(window, "$").andReturn(jqueryDummy);

            spyOn(jqueryDummy, "attr").andReturn("a s");
            spyOn(serverDummy, "getStatuses").andCallFake(function (a) {
                    a("ok", "123.456.789:10");
                }
            );
            spyOn(serverDummy2, "getStatuses").andCallFake(function (a) {
                    a("ok", "123.456.789:10");
                }
            );
            view.updateServerStatus();
            expect(jqueryDummy.attr).toHaveBeenCalledWith("class");
            expect(jqueryDummy.attr).toHaveBeenCalledWith("class", "dbserver " + "s" + " " + "ok");
            expect(jqueryDummy.attr).toHaveBeenCalledWith("class",
                "coordinator " + "s" + " " + "ok");
            expect(window.$).toHaveBeenCalledWith("#id123-456-789_10");

        });


        it("assert updateDBDetailList", function () {
            jqueryDummy = {
                find : function () {
                    return jqueryDummy;
                },
                attr : function () {
                },
                html : function () {
                    return jqueryDummy;
                },
                append : function () {
                    return jqueryDummy;
                },
                prop : function () {

                }
            };

            spyOn(serverDummy, "getList").andReturn(
                [
                    {name : "a", shards : {length : 1}},
                    {name : "b", shards : {length : 2}},
                    {name : "c", shards : {length : 3}}
                ]

            );


            spyOn(window, "$").andReturn(jqueryDummy);
            spyOn(jqueryDummy, "find").andCallThrough();
            spyOn(jqueryDummy, "attr").andReturn("dbName");
            spyOn(jqueryDummy, "html");
            spyOn(jqueryDummy, "append");
            spyOn(jqueryDummy, "prop");

            view.updateDBDetailList();


            expect(window.$).toHaveBeenCalledWith("#selectDB");
            expect(window.$).toHaveBeenCalledWith("#selectCol");

            expect(jqueryDummy.find).toHaveBeenCalledWith(":selected");
            expect(jqueryDummy.prop).toHaveBeenCalledWith("selected", true);
            expect(jqueryDummy.attr).toHaveBeenCalledWith("id");
            expect(jqueryDummy.html).toHaveBeenCalledWith("");
            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "c" + "\">" + "c" + "</option>"
            );
            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "a" + "\">" + "a" + "</option>"
            );

            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "b" + "\">" + "b" + "</option>"
            );



        });


        it("assert updateDBDetailList with selected db", function () {
            jqueryDummy = {
                find : function () {
                    return jqueryDummy;
                },
                attr : function () {
                },
                html : function () {
                    return jqueryDummy;
                },
                append : function () {
                    return jqueryDummy;
                },
                prop : function () {

                },
                length : 1
            };

            spyOn(serverDummy, "getList").andReturn(
                [
                    {name : "a", shards : {length : 1}},
                    {name : "b", shards : {length : 2}},
                    {name : "c", shards : {length : 3}}
                ]

            );


            spyOn(window, "$").andReturn(jqueryDummy);
            spyOn(jqueryDummy, "find").andCallThrough();
            spyOn(jqueryDummy, "attr").andReturn("dbName");
            spyOn(jqueryDummy, "html");
            spyOn(jqueryDummy, "append");
            spyOn(jqueryDummy, "prop");

            view.updateDBDetailList();


            expect(window.$).toHaveBeenCalledWith("#selectDB");
            expect(window.$).toHaveBeenCalledWith("#selectCol");

            expect(jqueryDummy.find).toHaveBeenCalledWith(":selected");
            expect(jqueryDummy.prop).toHaveBeenCalledWith("selected", true);
            expect(jqueryDummy.attr).toHaveBeenCalledWith("id");
            expect(jqueryDummy.html).toHaveBeenCalledWith("");
            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "c" + "\">" + "c" + "</option>"
            );
            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "a" + "\">" + "a" + "</option>"
            );

            expect(jqueryDummy.append).toHaveBeenCalledWith(
                "<option id=\"" + "b" + "\">" + "b" + "</option>"
            );



        });


        it("assert rerender", function () {

            spyOn(view, "updateServerStatus");
            spyOn(view, "getServerStatistics");
            spyOn(view, "updateServerTime");
            spyOn(view, "generatePieData").andReturn({data: "1"});
            spyOn(view, "renderPieChart");
            spyOn(view, "renderLineChart");
            spyOn(view, "updateDBDetailList");
            view.rerender();
            expect(view.updateServerStatus).toHaveBeenCalled();
            expect(view.getServerStatistics).toHaveBeenCalled();
            expect(view.updateServerTime).toHaveBeenCalled();
            expect(view.generatePieData).toHaveBeenCalled();
            expect(view.renderPieChart).toHaveBeenCalledWith({data: "1"});
            expect(view.renderLineChart).toHaveBeenCalled();
            expect(view.updateDBDetailList).toHaveBeenCalled();
        });


        it("assert render", function () {

            spyOn(view, "startUpdating");
            spyOn(view, "replaceSVGs");
            spyOn(view, "loadHistory");
            spyOn(view, "listByAddress").andReturn({data: "1"});
            spyOn(view, "generatePieData").andReturn({data: "1"});
            spyOn(view, "getServerStatistics");
            spyOn(view, "renderPieChart");
            spyOn(view, "renderLineChart");
            spyOn(view, "updateCollections");
            view.template = {
                render : function () {

                }
            };
            spyOn(serverDummy, "getList").andReturn([
                {name : "a", shards : {length : 1}},
                {name : "b", shards : {length : 2}},
                {name : "c", shards : {length : 3}}
                ]
            );
            spyOn(view.template, "render");
            view.render();
            expect(view.startUpdating).toHaveBeenCalled();
            expect(view.listByAddress).toHaveBeenCalled();
            expect(view.replaceSVGs).toHaveBeenCalled();
            expect(view.loadHistory).toHaveBeenCalled();
            expect(view.getServerStatistics).toHaveBeenCalled();
            expect(view.renderPieChart).toHaveBeenCalledWith({data: "1"});
            expect(view.template.render).toHaveBeenCalledWith({
                dbs: ["a", "b", "c"],
                byAddress: {data: "1"},
                type: "testPlan"
            });
            expect(view.generatePieData).toHaveBeenCalled();
            expect(view.renderLineChart).toHaveBeenCalled();
            expect(view.updateCollections).toHaveBeenCalled();
        });


        it("assert render with more than one address", function () {

            spyOn(view, "startUpdating");
            spyOn(view, "replaceSVGs");
            spyOn(view, "loadHistory");
            spyOn(view, "listByAddress").andReturn({data: "1", "ss" : 1});
            spyOn(view, "generatePieData").andReturn({data: "1"});
            spyOn(view, "getServerStatistics");
            spyOn(view, "renderPieChart");
            spyOn(view, "renderLineChart");
            spyOn(view, "updateCollections");
            view.template = {
                render : function () {

                }
            };
            spyOn(serverDummy, "getList").andReturn([
                {name : "a", shards : {length : 1}},
                {name : "b", shards : {length : 2}},
                {name : "c", shards : {length : 3}}
            ]
            );
            spyOn(view.template, "render");
            view.render();
            expect(view.startUpdating).toHaveBeenCalled();
            expect(view.listByAddress).toHaveBeenCalled();
            expect(view.replaceSVGs).toHaveBeenCalled();
            expect(view.loadHistory).toHaveBeenCalled();
            expect(view.getServerStatistics).toHaveBeenCalled();
            expect(view.renderPieChart).toHaveBeenCalledWith({data: "1"});
            expect(view.template.render).toHaveBeenCalledWith({
                dbs: ["a", "b", "c"],
                byAddress: {data: "1", "ss" : 1},
                type: "other"
            });
            expect(view.generatePieData).toHaveBeenCalled();
            expect(view.renderLineChart).toHaveBeenCalled();
            expect(view.updateCollections).toHaveBeenCalled();
        });


        it("assert generatePieData", function () {

            view.data = [
                {
                    get : function (a) {
                        if (a === "name") {
                            return "name1";
                        }
                        return {residentSize : "residentsize1"};
                    }
                },

                {
                    get : function (a) {
                        if (a === "name") {
                            return "name2";
                        }
                        return {residentSize : "residentsize2"};
                    }
                }
            ];

            expect(view.generatePieData()).toEqual([
                {
                    key : "name1",
                    value : "residentsize1",
                    time : view.serverTime
                },
                {
                    key : "name2",
                    value : "residentsize2",
                    time : view.serverTime
                }
                ]
            );

        });

        it("assert loadHistory", function () {
            var serverResult = [
                {
                    id : 1,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.789";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "heinz";}
                    }
                },
                {
                    id : 2,
                    get : function (a) {
                        if (a === "protocol") {return  "https";}
                        if (a === "address") {return  "123.456.799";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "herbert";}
                    }
                },
                {
                    id : 3,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.119";}
                        if (a === "status") {return  "notOk";}
                        if (a === "name") {return  "heinzle";}
                    }
                }
            ], serverResult2 = [
                {
                    id : 4,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.789";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "heinz";}
                    }
                },
                {
                    id : 5,
                    get : function (a) {
                        if (a === "protocol") {return  "https";}
                        if (a === "address") {return  "123.456.799";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "herbert";}
                    }
                },
                {
                    id : 6,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.119";}
                        if (a === "status") {return  "notOk";}
                        if (a === "name") {return  "heinzle";}
                    }
                }
            ];

            spyOn(serverDummy2, "findWhere").andReturn(serverResult[0]);


            view.dbservers = serverResult;
            spyOn(serverDummy, "forEach").andCallFake(function (a) {
                serverResult.forEach(a);
            });
            spyOn(serverDummy2, "forEach").andCallFake(function (a) {
                serverResult2.forEach(a);
            });
            spyOn(arangoDocumentsDummy, "getStatisticsHistory");
            view.loadHistory();

            expect(serverDummy2.findWhere).toHaveBeenCalledWith({
                status: "ok"
            });
            expect(arangoDocumentsDummy.getStatisticsHistory).toHaveBeenCalledWith({
                server: {
                    raw: "123.456.789",
                    isDBServer: true,
                    target: "heinz",
                    endpoint: "http://123.456.789",
                    addAuth: "authBinding"
                },
                figures: ["client.totalTime"]
            });
            expect(arangoDocumentsDummy.getStatisticsHistory).toHaveBeenCalledWith({
                server: {
                    raw: "123.456.799",
                    isDBServer: true,
                    target:  "herbert",
                    endpoint: "http://123.456.789",
                    addAuth: "authBinding"
                },
                figures: ["client.totalTime"]
            });
            expect(arangoDocumentsDummy.getStatisticsHistory).not.toHaveBeenCalledWith({
                server: {
                    raw: "123.456.119",
                    isDBServer: true,
                    target:  "heinzle",
                    endpoint: "http://123.456.789",
                    addAuth: "authBinding"
                },
                figures: ["client.totalTime"]
            });
            expect(view.hist).toEqual({
                1: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4},
                2: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4},
                4: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4},
                5: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4}}
            );


        });



        it("assert getServerStatistics", function () {
            var clusterStatisticsCollectionDummy = {
                add : function (a) {
                },
                fetch : function () {

                },
                forEach : function () {

                }
            }, serverResult = [
                {
                    id : 1,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.789";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "heinz";}
                    }
                },
                {
                    id : 2,
                    get : function (a) {
                        if (a === "protocol") {return  "https";}
                        if (a === "address") {return  "123.456.799";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "herbert";}
                    }
                },
                {
                    id : 3,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.119";}
                        if (a === "status") {return  "notOk";}
                        if (a === "name") {return  "heinzle";}
                    }
                }
            ], serverResult2 = [
                {
                    id : 4,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.789";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "heinz";}
                    }
                },
                {
                    id : 5,
                    get : function (a) {
                        if (a === "protocol") {return  "https";}
                        if (a === "address") {return  "123.456.799";}
                        if (a === "status") {return  "ok";}
                        if (a === "name") {return  "herbert";}
                    }
                },
                {
                    id : 6,
                    get : function (a) {
                        if (a === "protocol") {return  "http";}
                        if (a === "address") {return  "123.456.119";}
                        if (a === "status") {return  "notOk";}
                        if (a === "name") {return  "heinzle";}
                    }
                }
                ], clusterStatistics = [

                {
                    get : function (a) {
                        if (a === "server") {return  {
                            uptime : 100
                        };}
                        if (a === "client") {return  {
                            totalTime : {
                                count : 0,
                                sum : 10
                            }
                        };}
                        if (a === "name") {return  "herbert";}
                    }
                },

                {
                    get : function (a) {
                        if (a === "server") {return  {
                            uptime : 10
                        };}
                        if (a === "client") {return  {
                            totalTime : {
                                count : 1,
                                sum : 10
                            }
                        };}
                        if (a === "name") {return  "heinz";}
                    }
                }
            ], foundUrls = [];



            spyOn(window, "ClusterStatisticsCollection").andReturn(
                clusterStatisticsCollectionDummy
            );
            spyOn(window, "Statistics").andReturn({});
            spyOn(serverDummy2, "forEach").andCallFake(function (a) {
                serverResult2.forEach(a);
            });
            spyOn(clusterStatisticsCollectionDummy, "add").andCallFake(function (y) {
                foundUrls.push(y.url);
            });
            spyOn(clusterStatisticsCollectionDummy, "fetch");
            spyOn(clusterStatisticsCollectionDummy, "forEach").andCallFake(function (a) {
                clusterStatistics.forEach(a);
            });

            spyOn(serverDummy2, "first").andReturn({
                id : 1,
                get : function (a) {
                    if (a === "protocol") {return  "http";}
                    if (a === "address") {return  "123.456.789";}
                    if (a === "status") {return  "ok";}
                    if (a === "name") {return  "heinz";}
                }
            });
            view.hist = {

            };
            view.hist.heinz = {
                lastTime : 10
            };
            view.hist.herbert = {
                lastTime : view.serverTime
            };


            view.dbservers = serverResult;


            view.getServerStatistics();

            expect(serverDummy2.first).toHaveBeenCalled();
            expect(foundUrls.indexOf(
                "http://123.456.789/_admin/clusterStatistics?DBserver=heinz")
            ).not.toEqual(-1);
            expect(foundUrls.indexOf(
                "http://123.456.789/_admin/clusterStatistics?DBserver=herbert"
            )).not.toEqual(-1);
            expect(foundUrls.indexOf(
                "http://123.456.789/_admin/clusterStatistics?DBserver=heinzle"
            )).toEqual(-1);


            expect(foundUrls.indexOf("http://123.456.789/_admin/statistics")).not.toEqual(-1);
            expect(foundUrls.indexOf("https://123.456.799/_admin/statistics")).not.toEqual(-1);
            expect(foundUrls.indexOf("http://123.456.119/_admin/statistics")).toEqual(-1);


        });



        it("assert renderPieChart", function () {

            var d3Dummy = {

                scale : {
                    category20 : function () {

                    }
                },
                svg : {
                    arc : function () {
                        return d3Dummy.svg;
                    },
                    outerRadius : function () {
                        return d3Dummy.svg;
                    },
                    innerRadius : function () {
                        return {
                            centroid : function (a) {
                                return a;
                            }
                        };
                    }
                },
                layout : {
                    pie : function () {
                        return d3Dummy.layout;
                    },
                    sort : function (a) {
                        expect(a({value : 1})).toEqual(1);
                        return d3Dummy.layout;
                    },
                    value : function (a) {
                        expect(a({value : 1})).toEqual(1);
                        return d3Dummy.layout.pie;
                    }
                },
                select : function () {
                    return d3Dummy;
                },
                remove : function () {

                },
                append : function () {
                    return d3Dummy;
                },
                attr : function (a, b) {
                    if (a === "transform" && typeof b === 'function') {
                        expect(b(1)).toEqual("translate(1)");
                    }
                    return d3Dummy;
                },
                selectAll: function () {
                    return d3Dummy;
                },
                data: function () {
                    return d3Dummy;
                },
                enter: function () {
                    return d3Dummy;
                },
                style : function (a, b) {
                    if (typeof b === 'function') {
                        expect(b("item", 1)).toEqual("white");
                    }
                    return d3Dummy;
                },
                text : function (a) {
                    a({data : {
                        key : "1",
                        value : 2
                    }});
                }



            };

            spyOn(d3.scale, "category20").andReturn(function (i) {
                var l = ["red", "white"];
                return l[i];
            });
            spyOn(d3.svg, "arc").andReturn(d3Dummy.svg.arc());
            spyOn(d3.layout, "pie").andReturn(d3Dummy.layout.pie());
            spyOn(d3, "select").andReturn(d3Dummy.select());

            view.renderPieChart();


        });
        /*
             renderPieChart: function(dataset) {
                            var w = 280;
                            var h = 160;
                            var radius = Math.min(w, h) / 2; //change 2 to 1.4. It's hilarious.
                            var color = d3.scale.category20();

                            var arc = d3.svg.arc() //each datapoint will create one later.
                                .outerRadius(radius - 20)
                                .innerRadius(0);
                            var pie = d3.layout.pie()
                                .sort(function (d) {
                                    return d.value;
                                })
                                .value(function (d) {
                                    return d.value;
                                });
                            d3.select("#clusterGraphs").select("svg").remove();
                            var pieChartSvg = d3.select("#clusterGraphs").append("svg")
                                .attr("width", w)
                                .attr("height", h)
                                .attr("class", "clusterChart")
                                .append("g") //someone to transform. Groups data.
                                .attr("transform", "translate(" + w / 2 + "," + h / 2 + ")");

                            var arc2 = d3.svg.arc()
                                .outerRadius(radius-2)
                                .innerRadius(radius-2);

                            var slices = pieChartSvg.selectAll(".arc")
                                .data(pie(dataset))
                                .enter().append("g")
                                .attr("class", "slice");

                            */
/*jslint unparam: true*//*

                    slices.append("path")
                        .attr("d", arc)
                        .style("fill", function (item, i) {
                            return color(i);
                        });
                    */
/*jslint unparam: false*//*

                    slices.append("text")
                        .attr("transform", function(d) { return "translate(" +
                        arc.centroid(d) + ")"; })
                        .attr("dy", ".35em")
                        .style("text-anchor", "middle")
                        .text(function(d) {
                            var v = d.data.value / 1000000000;
                            return v.toFixed(2) + "GB"; });

                    slices.append("text")
                        .attr("transform", function(d) { return "translate(" +
                         arc2.centroid(d) + ")"; })
                        .attr("dy", ".35em")
                        .style("text-anchor", "middle")
                        .text(function(d) { return d.data.key; });
                },

                renderLineChart: function(remake) {
                    var self = this;
                    self.chartData = {
                        labelsNormal : ['datetime'],
                        labelsShowAll : ['datetime'],
                        data : [],
                        visibilityNormal : [],
                        visibilityShowAll : []
                    };
                    var getData = function() {
                        var data = {};
                        Object.keys(self.hist).forEach(function(server) {
                            Object.keys(self.hist[server]).forEach(function(date) {
                                if (date === "lastTime") {
                                    return;
                                }
                                if (!data[date]) {
                                    data[date] = {};
                                    Object.keys(self.hist).forEach(function(s) {
                                        data[date][s] = null;
                                    });
                                }
                                data[date][server] = self.hist[server][date];
                            });
                        });
                        Object.keys(data).forEach(function(d) {
                            var i = 0;
                            var sum = 0;
                            Object.keys(data[d]).forEach(function(server) {
                                if (data[d][server] !== null) {
                                    i++;
                                    sum = sum + data[d][server];
                                }
                                data[d].ClusterAverage = sum / i;
                            });
                        });
                        Object.keys(data).sort().forEach(function (time) {
                            var dataList = [new Date(parseFloat(time))];
                            self.max = Number.NEGATIVE_INFINITY;
                            self.chartData.visibilityShowAll = [];
                            self.chartData.labelsShowAll = [ "Date"];
                            Object.keys(data[time]).sort().forEach(function (server) {
                                self.chartData.visibilityShowAll.push(true);
                                self.chartData.labelsShowAll.push(server);
                                dataList.push(data[time][server]);
                            });
                            self.chartData.data.push(dataList);
                        });
                        var latestEntry = self.chartData.data[self.chartData.data.length -1];
                        latestEntry.forEach(function (e) {
                            if (latestEntry.indexOf(e) > 0) {
                                if (e !== null) {
                                    if (self.max < e) {
                                        self.max = e;
                                    }
                                }
                            }
                        });
                        self.chartData.visibilityNormal = [];
                        self.chartData.labelsNormal = [ "Date"];
                        var i = 0;
                        latestEntry.forEach(function (e) {
                            if (i > 0) {
                                if ("ClusterAverage" === self.chartData.labelsShowAll[i]) {
                                    self.chartData.visibilityNormal.push(true);
                                    self.chartData.labelsNormal.push(
                                        self.chartData.labelsShowAll[i] + " (avg)"
                                    );
                                } else if (e === self.max ) {
                                    self.chartData.visibilityNormal.push(true);
                                    self.chartData.labelsNormal.push(
                                        self.chartData.labelsShowAll[i] + " (max)"
                                    );
                                } else {
                                    self.chartData.visibilityNormal.push(false);
                                    self.chartData.labelsNormal.push(self.chartData.
                                    labelsShowAll[i]);
                                }
                            }
                            i++;
                        });
                    };
                    if (this.graph !== undefined && !remake) {
                        getData();
                        var opts = {file : this.chartData.data};
                        if (this.graphShowAll ) {
                            opts.labels = this.chartData.labelsShowAll;
                            opts.visibility = this.chartData.visibilityShowAll;
                        } else {
                            opts.labels = this.chartData.labelsNormal;
                            opts.visibility = this.chartData.visibilityNormal;
                        }
                        opts.dateWindow = this.updateDateWindow( this.graph.graph,
                        this.graphShowAll);
                        this.graph.graph.updateOptions(opts);
                        return;
                    }

                    var makeGraph = function(remake) {
                        self.graph = {data : null, options :
                            self.dygraphConfig.getDefaultConfig("clusterAverageRequestTime")
                        };
                        getData();
                        self.graph.data = self.chartData.data;
                        self.graph.options.visibility = self.chartData.visibilityNormal;
                        self.graph.options.labels = self.chartData.labelsNormal;
                        self.graph.options.colors =
                            self.dygraphConfig.getColors(self.chartData.labelsNormal);
                        if (remake) {
                            self.graph.options =
                                self.dygraphConfig.getDetailChartConfig(
                                "clusterAverageRequestTime");
                            self.graph.options.labels = self.chartData.labelsShowAll;
                            self.graph.options.colors =
                                self.dygraphConfig.getColors(self.chartData.labelsShowAll);
                            self.graph.options.visibility = self.chartData.visibilityShowAll;
                            self.graph.options.height = $('.modal-chart-detail').height() * 0.7;
                            self.graph.options.width = $('.modal-chart-detail').width() * 0.84;
                            self.graph.options.title = "";
                        }
                        self.graph.graph = new Dygraph(
                            document.getElementById(remake ? 'lineChartDetail' : 'lineGraph'),
                            self.graph.data,
                            self.graph.options
                        );
                        self.graph.graph.setSelection(false, 'ClusterAverage', true);
                    };
                    makeGraph(remake);

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

                stopUpdating: function () {
                    window.clearTimeout(this.timer);
                    delete this.graph;
                    this.isUpdating = false;
                },

                startUpdating: function () {
                    if (this.isUpdating) {
                        return;
                    }
                    this.isUpdating = true;
                    var self = this;
                    this.timer = window.setInterval(function() {
                        self.rerender();
                    }, this.interval);
                },


                dashboard: function(e) {
                    this.stopUpdating();
                    var tar = $(e.currentTarget);
                    var serv = {};
                    var cur;
                    var coord;
                    $("#waitModalLayer").remove();
                    var ip_port = tar.attr("id");
                    ip_port = ip_port.replace(/\-/g,'.');
                    ip_port = ip_port.replace(/\_/g,':');
                    ip_port = ip_port.substr(2);
                    serv.raw = ip_port;
                    serv.isDBServer = tar.hasClass("dbserver");
                    if (serv.isDBServer) {
                        cur = this.dbservers.findWhere({
                            address: serv.raw
                        });
                        coord = this.coordinators.findWhere({
                            status: "ok"
                        });
                        serv.endpoint = coord.get("protocol")
                            + "://"
                            + coord.get("address");
                    } else {
                        cur = this.coordinators.findWhere({
                            address: serv.raw
                        });
                        serv.endpoint = cur.get("protocol")
                            + "://"
                            + cur.get("address");
                    }
                    serv.target = encodeURIComponent(cur.get("name"));
                    window.App.serverToShow = serv;
                    window.App.dashboard();
                },

                showDetail : function() {
                    var self = this;
                    delete self.graph;
                    window.modalView.hideFooter = true;
                    window.modalView.hide();
                    window.modalView.show(
                        "modalGraph.ejs",
                        "Average request time in milliseconds",
                        undefined,
                        undefined,
                        undefined
                    );

                    window.modalView.hideFooter = false;

                    $('#modal-dialog').on('hidden', function () {
                        delete self.graph;
                        self.resetShowAll();
                    });
                    //$('.modal-body').css({"max-height": "100%" });
                    $('#modal-dialog').toggleClass("modal-chart-detail", true);
                    self.setShowAll();
                    self.renderLineChart(true);
                    return self;
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

                resize: function () {
                    var dimensions;
                    if (this.graph) {
                        dimensions = this.getCurrentSize(this.graph.graph.maindiv_.id);
                        this.graph.graph.resize(dimensions.width, dimensions.height);
                    }
                }
            });
*/


    });

}());

