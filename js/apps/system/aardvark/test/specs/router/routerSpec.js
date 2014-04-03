/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, arangoHelper, afterEach, it, console, spyOn, expect*/
/*global $, jasmine, _*/

(function () {
    "use strict";

    describe("The router", function () {

        var
            fakeDB,
            graphDummy,
            storeDummy,
            naviDummy,
            footerDummy,
            documentDummy,
            documentsDummy,
            sessionDummy,
            graphsDummy,
            foxxDummy,
            logsDummy,
            statisticBarDummy,
            collectionsViewDummy,
            databaseDummy,
            userBarDummy,
            documentViewDummy,
            statisticsDescriptionCollectionDummy,
            statisticsCollectionDummy,
            logsViewDummy,
            dashboardDummy,
            documentsViewDummy;

        // Spy on all views that are initialized by startup
        beforeEach(function () {
            naviDummy = {
                id: "navi",
                render: function () {
                },
                handleResize: function () {
                },
                selectMenuItem: function () {
                },
                handleSelectDatabase: function () {
                }
            };
            footerDummy = {
                id: "footer",
                render: function () {
                },
                handleResize: function () {
                },
                system: {}
            };
            documentDummy = {id: "document"};
            documentsDummy = {id: "documents"};
            graphDummy = {
                id: "graph",
                handleResize: function () {
                },
                render: function () {
                }
            };
            storeDummy = {
                id: "store",
                fetch: function (a) {
                    if (a && a.success) {
                        a.success();
                    }
                }
            };
            graphsDummy = {id: "graphs"};
            logsDummy = {
                id: "logs",
                fetch: function (a) {
                    a.success();

                }
            };

            dashboardDummy = {
                id: "dashboard",
                resize: function () {
                },
                stopUpdating: function () {
                },
                render: function () {
                }
            };
            statisticsDescriptionCollectionDummy = {
                id: "statisticsDescription",
                fetch: function () {
                }
            };
            statisticsCollectionDummy = {
                id: "statisticsCollection",
                fetch: function () {
                }
            };
            databaseDummy = {
                id: "databaseCollection",
                fetch: function () {
                }
            };
            fakeDB = {
                id: "fakeDB",
                fetch: function () {
                }
            };
            sessionDummy = {
                id: "sessionDummy"
            };
            foxxDummy = {
                id: "foxxDummy",
                fetch: function () {
                },
                findWhere: function (a) {
                    return a._key;
                }
            };
            statisticBarDummy = {
                id: "statisticBarDummy",
                render: function () {
                }
            };
            userBarDummy = {
                id: "userBarDummy",
                render: function () {
                }
            };
            collectionsViewDummy = {
                render: function () {
                }
            };
            documentViewDummy = {
                render: function () {
                },
                typeCheck: function () {
                }
            };
            documentsViewDummy = {
                render: function () {
                },
                initTable: function () {
                }
            };

            logsViewDummy = {
                render: function () {
                },
                initLogTables: function () {
                },
                drawTable: function () {
                }
            };


            spyOn(window, "DocumentsView").andReturn(documentsViewDummy);
            spyOn(window, "DocumentView").andReturn(documentViewDummy);
            spyOn(window, "arangoCollections").andReturn(storeDummy);
            spyOn(window, "ArangoUsers").andReturn(sessionDummy);
            spyOn(window, "arangoDocuments").andReturn(documentsDummy);
            spyOn(window, "arangoDocument").andReturn(documentDummy);
            spyOn(window, "ArangoDatabase").andReturn(databaseDummy);
            spyOn(window, "GraphCollection").andReturn(graphsDummy);
            spyOn(window, "FoxxCollection").andReturn(foxxDummy);
            spyOn(window, "StatisticsDescriptionCollection").andReturn(
                statisticsDescriptionCollectionDummy
            );
            spyOn(window, "StatisticsCollection").andReturn(statisticsCollectionDummy);
            spyOn(window, "CollectionsView").andReturn(collectionsViewDummy);
            spyOn(window, "LogsView").andReturn(logsViewDummy);
            spyOn(window, "ArangoLogs").andReturn(logsDummy);
            spyOn(window, "FooterView").andReturn(footerDummy);
            spyOn(footerDummy, "render");
            spyOn(window, "NavigationView").andReturn(naviDummy);
            spyOn(naviDummy, "render");
            spyOn(naviDummy, "selectMenuItem");
            spyOn(window, "GraphView").andReturn(graphDummy);
            spyOn(window, "CurrentDatabase").andReturn(fakeDB);
            spyOn(fakeDB, "fetch").andCallFake(function (options) {
                expect(options.async).toBeFalsy();
            });
            spyOn(window, "DBSelectionView");
            spyOn(window, "dashboardView").andReturn(dashboardDummy);
            spyOn(window, "StatisticBarView").andReturn(statisticBarDummy);
            spyOn(window, "UserBarView").andReturn(userBarDummy);
        });

        describe("initialisation", function () {

            var r;


            beforeEach(function () {
                spyOn(storeDummy, "fetch");
                spyOn($, "ajax").andCallFake(function (opt) {
                    if (opt.url ==="https://www.arangodb.org/repositories/versions." +
                        "php?callback=parseVersions") {
                        opt.success("success");
                    }
                });
                r = new window.Router();
            });
            it("should create a Foxx Collection", function () {
                expect(window.FoxxCollection).toHaveBeenCalled();
            });

            it("should delete the dashboardview", function () {
                var e = new window.Router();
                spyOn(e, "bind").andCallFake(function (a, func) {
                    expect(a).toEqual("all");
                    func("route", "dashboard");
                    expect(e.dashboardView).toEqual(undefined);
                    expect(e.currentRoute).toEqual("dashboard");
                });
                e.initialize();
                expect(e.bind).toHaveBeenCalled();
            });

            it("should stop the updating of the dashboardview", function () {
                var e = new window.Router();
                e.dashboardView = dashboardDummy;
                spyOn(dashboardDummy, "stopUpdating");
                e.currentRoute = "dashboard";
                spyOn(e, "bind").andCallFake(function (a, func) {
                    expect(a).toEqual("all");
                    func("route", "dashboard");
                    expect(dashboardDummy.stopUpdating).toHaveBeenCalled();
                    expect(e.currentRoute).toEqual("dashboard");
                });
                e.initialize();
                expect(window.LogsView).toHaveBeenCalledWith({collection: logsDummy});
                expect(e.bind).toHaveBeenCalled();
            });


            it("should bind a resize event and call the handle Resize Method", function () {
                var e = new window.Router();
                spyOn(e, "handleResize");
                spyOn(window, "$").andReturn({
                    resize: function (a) {
                        a();
                    }
                });
                e.initialize();
                //expect($(window).resize).toHaveBeenCalledWith(jasmine.any(Function));
                expect(e.handleResize).toHaveBeenCalled();
            });

            it("should create a graph view", function () {
                expect(window.GraphView).toHaveBeenCalledWith({
                    collection: storeDummy,
                    graphs: graphsDummy
                });
            });

            it("should create a documentView", function () {
                expect(window.DocumentView).toHaveBeenCalledWith(
                    {
                        collection: documentDummy
                    }
                );
            });

            it("should create a documentsView", function () {
                expect(window.DocumentsView).toHaveBeenCalled();
            });

            it("should create collectionsView", function () {
                expect(window.CollectionsView).toHaveBeenCalledWith({
                    collection: storeDummy
                });
            });

            it("should fetch the collectionsStore", function () {
                expect(storeDummy.fetch).toHaveBeenCalled();
            });

            it("should fetch the current db", function () {
                expect(window.CurrentDatabase).toHaveBeenCalled();
                expect(fakeDB.fetch).toHaveBeenCalled();
            });
        });

        describe("navigation", function () {

            var r,
                simpleNavigationCheck;

            beforeEach(function () {
                r = new window.Router();
                simpleNavigationCheck =
                    function (url, viewName, navTo, initObject,
                              funcList, shouldNotRender, shouldNotCache) {
                    var route,
                        view = {},
                        checkFuncExec = function () {
                            _.each(funcList, function (v, f) {
                                if (v !== undefined) {
                                    expect(view[f]).toHaveBeenCalledWith(v);
                                } else {
                                    expect(view[f]).toHaveBeenCalled();
                                }
                            });
                        };
                    funcList = funcList || {};
                    if (_.isObject(url)) {
                        route = function () {
                            r[r.routes[url.url]].apply(r, url.params);
                        };
                    } else {
                        route = r[r.routes[url]].bind(r);
                    }
                    if (!funcList.hasOwnProperty("render") && !shouldNotRender) {
                        funcList.render = undefined;
                    }
                    _.each(_.keys(funcList), function (f) {
                        view[f] = function () {
                        };
                        spyOn(view, f);
                    });
                    expect(route).toBeDefined();
                    try {
                        spyOn(window, viewName).andReturn(view);
                    } catch (e) {
                        if (e.message !== viewName + ' has already been spied upon') {
                            throw e;
                        }
                    }
                    route();
                    if (initObject) {
                        expect(window[viewName]).toHaveBeenCalledWith(initObject);
                    } else {
                        expect(window[viewName]).toHaveBeenCalled();
                    }
                    checkFuncExec();
                    if (navTo) {
                        expect(naviDummy.selectMenuItem).toHaveBeenCalledWith(navTo);
                    }
                    // Check if the view is loaded from cache
                    if (shouldNotCache !== true) {
                        window[viewName].reset();
                        _.each(_.keys(funcList), function (f) {
                            view[f].reset();
                        });
                        naviDummy.selectMenuItem.reset();
                        route();
                        if (!r.hasOwnProperty(viewName)) {
                            expect(window[viewName]).not.toHaveBeenCalled();
                        }
                        checkFuncExec();
                    }
                };
            });

            /*
             routes: {
             ""                                    : "dashboard",
             "dashboard"                           : "dashboard",
             "collection/:colid"                   : "collection",
             "collections"                         : "collections",
             "collectionInfo/:colid"               : "collectionInfo",
             "new"                                 : "newCollection",
             "login"                               : "login",
             "collection/:colid/documents/:pageid" : "documents",
             "collection/:colid/:docid"            : "document",
             "shell"                               : "shell",
             "query"                               : "query",
             "logs"                                : "logs",
             "api"                                 : "api",
             "databases"                           : "databases",
             "application/installed/:key"          : "applicationEdit",
             "application/available/:key"          : "applicationInstall",
             "applications"                        : "applications",
             "application/documentation/:key"      : "appDocumentation",
             "graph"                               : "graph",
             "graphManagement"                     : "graphManagement",
             "graphManagement/add"                 : "graphAddNew",
             "graphManagement/delete/:name"        : "graphDelete",
             "userManagement"                      : "userManagement",
             "userProfile"                         : "userProfile",
             "testing"                             : "testview"
             },
             */

            it("should offer all necessary routes", function () {
                var available = _.keys(r.routes),
                    expected = [
                        "",
                        "dashboard",
                        "collection/:colid",
                        "collections",
                        "collectionInfo/:colid",
                        "new",
                        "login",
                        "collection/:colid/documents/:pageid",
                        "collection/:colid/:docid" ,
                        "shell",
                        "query",
                        "logs",
                        "api",
                        "databases",
                        "application/installed/:key",
                        "application/available/:key",
                        "applications",
                        "application/documentation/:key",
                        "graph",
                        "graphManagement",
                        "graphManagement/add",
                        "graphManagement/delete/:name",
                        "userManagement",
                        "userProfile" ,
                        "testing"
                    ],
                    diff = _.difference(available, expected);
                this.addMatchers({
                    toDefineTheRoutes: function (exp) {
                        var avail = this.actual,
                            leftDiff = _.difference(avail, exp),
                            rightDiff = _.difference(exp, avail);
                        this.message = function () {
                            var msg = "";
                            if (rightDiff.length) {
                                msg += "Expect routes: "
                                    + rightDiff.join(' & ')
                                    + " to be available.\n";
                            }
                            if (leftDiff.length) {
                                msg += "Did not expect routes: "
                                    + leftDiff.join(" & ")
                                    + " to be available.";
                            }
                            return msg;
                        };
                        return true;
                    }
                });
                expect(available).toDefineTheRoutes(expected);
            });

            it("should route to a collection", function () {
                var colid = 5;
                simpleNavigationCheck(
                    {
                        url: "collection/:colid",
                        params: [colid]
                    },
                    "CollectionView",
                    "collections-menu",
                    undefined,
                    {
                        setColId: colid
                    }
                );
            });

            it("should route to collection info", function () {
                var colid = 5;
                simpleNavigationCheck(
                    {
                        url: "collectionInfo/:colid",
                        params: [colid]
                    },
                    "CollectionInfoView",
                    "collections-menu",
                    undefined,
                    {
                        setColId: colid
                    }
                );
            });
            it("should route to documents", function () {
                var colid = 5,
                    pageid = 6;
                documentsDummy.getDocuments = function () {
                };
                spyOn(documentsDummy, "getDocuments");
                spyOn(window.documentsView, "render");
                spyOn(arangoHelper, "collectionApiType").andReturn(1);
                simpleNavigationCheck(
                    {
                        url: "collection/:colid/documents/:pageid",
                        params: [colid, pageid]
                    },
                    "DocumentsView",
                    "",
                    undefined,
                    {
                    },
                    true
                );
                expect(window.documentsView.colid).toEqual(colid);
                expect(window.documentsView.collectionID).toEqual(colid);
                expect(window.documentsView.pageid).toEqual(pageid);
                expect(window.documentsView.type).toEqual(1);
                expect(window.documentsView.render).toHaveBeenCalled();
                expect(arangoHelper.collectionApiType).toHaveBeenCalledWith(colid);
                expect(documentsDummy.getDocuments).toHaveBeenCalledWith(colid, pageid);
            });

            it("should route to document", function () {
                var colid = 5,
                    docid = 6;
                spyOn(arangoHelper, "collectionApiType").andReturn(5);
                spyOn(window.documentView, "render");
                simpleNavigationCheck(
                    {
                        url: "collection/:colid/:docid",
                        params: [colid, docid]
                    },
                    "DocumentView",
                    "",
                    undefined,
                    {
                    },
                    true
                );
                expect(window.documentView.colid).toEqual(colid);
                expect(window.documentView.docid).toEqual(docid);
                expect(window.documentView.type).toEqual(5);
                expect(window.documentView.render).toHaveBeenCalled();
                expect(arangoHelper.collectionApiType).toHaveBeenCalledWith(colid);
            });


            it("should route to databases", function () {
                spyOn(arangoHelper, "databaseAllowed").andReturn(true);
                simpleNavigationCheck(
                    "databases",
                    "databaseView",
                    "databases-menu",
                    {
                        collection: databaseDummy
                    }
                );
            });

            it("should not route to databases and hide database navi", function () {
                spyOn(arangoHelper, "databaseAllowed").andReturn(false);
                spyOn(r, "navigate");
                var jQueryDummy = {
                    id: "jquery",
                    css: function () {
                    }
                };
                spyOn(window, "$").andReturn(jQueryDummy);
                spyOn(jQueryDummy, "css");

                r.databases();
                expect(r.navigate).toHaveBeenCalledWith("#", {trigger: true});
                expect(naviDummy.selectMenuItem).toHaveBeenCalledWith("dashboard-menu");
                expect(jQueryDummy.css).toHaveBeenCalledWith('display', 'none');
            });

            it("should not fetch logs , not allowed", function () {
                spyOn(r, "navigate");
                window.currentDB = new window.DatabaseModel({name: "system"});
                r.logs();
                expect(r.navigate).toHaveBeenCalledWith('', { trigger: true });
            });

            it("should fetch logs", function () {
                spyOn(r, "navigate");
                var jQueryDummy = {
                    id: "jquery",
                    tab: function () {
                    },
                    click: function () {
                    }
                };
                spyOn(window, "$").andReturn(jQueryDummy);
                spyOn(jQueryDummy, "tab");
                spyOn(jQueryDummy, "click");

                spyOn(logsViewDummy, "render");
                spyOn(logsViewDummy, "initLogTables");
                spyOn(logsViewDummy, "drawTable");
                window.currentDB = new window.DatabaseModel({name: "_system"});
                simpleNavigationCheck(
                    "logs",
                    "LogsView",
                    "tools-menu",
                    undefined,
                    {
                    },
                    true
                );
                expect(jQueryDummy.tab).toHaveBeenCalledWith('show');
                expect(jQueryDummy.click).toHaveBeenCalled();
                expect(logsViewDummy.render).toHaveBeenCalled();
                expect(logsViewDummy.initLogTables).toHaveBeenCalled();
                expect(logsViewDummy.drawTable).toHaveBeenCalled();
            });

            it("should navigate to the dashboard", function () {
                delete r.statisticsDescriptionCollection;
                delete r.statisticsCollection;
                delete r.dashboardView;
                spyOn(statisticsDescriptionCollectionDummy, "fetch");
                simpleNavigationCheck(
                    "dashboard",
                    "dashboardView",
                    "dashboard-menu",
                    {
                        collection: statisticsCollectionDummy,
                        description: statisticsDescriptionCollectionDummy,
                        documentStore: window.arangoDocumentsStore,
                        dygraphConfig: window.dygraphConfig
                    },
                    {
                    },
                    true
                );
                expect(statisticsDescriptionCollectionDummy.fetch).toHaveBeenCalledWith({
                    async: false
                });
            });

            it("should navigate to the graphView", function () {
                simpleNavigationCheck(
                    "graph",
                    "GraphView",
                    "graphviewer-menu",
                    {
                        graphs: { id: 'graphs' },
                        collection: storeDummy
                    },
                    {
                    },
                    true
                );
            });

            it("should navigate to the editAppView", function () {
                var key = 5;
                spyOn(foxxDummy, "findWhere").andCallThrough();
                spyOn(foxxDummy, "fetch");
                simpleNavigationCheck(
                    {
                        url: "application/installed/:key",
                        params: [key]
                    },
                    "foxxEditView",
                    undefined,
                    {
                        model: key
                    },
                    {
                    },
                    false,
                    true
                );
                expect(foxxDummy.fetch).toHaveBeenCalledWith({async: false});
                expect(foxxDummy.findWhere).toHaveBeenCalledWith({_key: 5});
            });

            it("should navigate to the installAppView", function () {
                var key = 5;
                spyOn(foxxDummy, "fetch");
                simpleNavigationCheck(
                    {
                        url: "application/available/:key",
                        params: [key]
                    },
                    "foxxMountView",
                    undefined,
                    {
                        collection: foxxDummy
                    },
                    {
                    },
                    false
                );
                expect(foxxDummy.fetch).toHaveBeenCalledWith({async: false});
            });

            it("should navigate to the appDocumentation", function () {
                var key = 5;
                simpleNavigationCheck(
                    {
                        url: "application/documentation/:key",
                        params: [key]
                    },
                    "AppDocumentationView",
                    "applications-menu",
                    {
                        key: 5
                    },
                    {
                    },
                    false,
                    true
                );
            });


            it("should navigate to the appDocumentation", function () {
                var key = 5;
                simpleNavigationCheck(
                    {
                        url: "application/documentation/:key",
                        params: [key]
                    },
                    "AppDocumentationView",
                    "applications-menu",
                    {
                        key: 5
                    },
                    {
                    },
                    false,
                    true
                );
            });

            it("should call handleSelectDatabase in naviview", function () {
                spyOn(naviDummy, "handleSelectDatabase");
                r.handleSelectDatabase();
                expect(naviDummy.handleSelectDatabase).toHaveBeenCalled();
            });

            it("should handle resizing", function () {
                spyOn(graphDummy, "handleResize");
                spyOn($.fn, 'width').andReturn(500);

                r.handleResize();
                expect(graphDummy.handleResize).toHaveBeenCalledWith(241);
            });


            it("should navigate to the userManagement", function () {
                var key = 5;
                simpleNavigationCheck(
                    "userManagement",
                    "userManagementView",
                    "tools-menu",
                    {
                        collection: window.userCollection
                    },
                    {
                    },
                    false
                );
            });

            it("should navigate to the userProfile", function () {
                var key = 5;
                simpleNavigationCheck(
                    "userProfile",
                    "userManagementView",
                    "tools-menu",
                    {
                        collection: window.userCollection
                    },
                    {
                    },
                    false
                );
            });

            it("should route to the login screen", function () {
                simpleNavigationCheck(
                    "login",
                    "loginView",
                    "",
                    {collection: sessionDummy}
                );
            });

            it("should route to the testView screen", function () {
                simpleNavigationCheck(
                    "testing",
                    "testView",
                    ""
                );
            });

            it("should route to the graph management tab", function () {
                simpleNavigationCheck(
                    "new",
                    "newCollectionView",
                    "collections-menu",
                    {}
                );
            });

            it("should route to the api tab", function () {
                simpleNavigationCheck(
                    "api",
                    "apiView",
                    "tools-menu"
                );
            });

            it("should route to the query tab", function () {
                simpleNavigationCheck(
                    "query",
                    "queryView",
                    "query-menu"
                );
            });

            it("should route to the shell tab", function () {
                simpleNavigationCheck(
                    "shell",
                    "shellView",
                    "tools-menu"
                );
            });

            it("should route to the graph management tab", function () {
                simpleNavigationCheck(
                    "graphManagement",
                    "GraphManagementView",
                    "graphviewer-menu",
                    { collection: graphsDummy}
                );
            });

            it("should offer the add new graph view", function () {
                simpleNavigationCheck(
                    "graphManagement/add",
                    "AddNewGraphView",
                    "graphviewer-menu",
                    {
                        collection: storeDummy,
                        graphs: graphsDummy
                    }
                );
            });

            it("should offer the delete graph view", function () {
                var name = "testGraph";
                simpleNavigationCheck(
                    {
                        url: "graphManagement/delete/:name",
                        params: [name]
                    },
                    "DeleteGraphView",
                    "graphviewer-menu",
                    {
                        collection: graphsDummy
                    },
                    {
                        render: name
                    }
                );
            });

            it("should route to the applications tab", function () {
                simpleNavigationCheck(
                    "applications",
                    "ApplicationsView",
                    "applications-menu",
                    { collection: foxxDummy},
                    {
                        reload: undefined
                    },
                    true
                );
            });

        });

        describe("initVersionCheck", function () {

            var r;


            beforeEach(function () {
                r = new window.Router();
            });

            it("initVersionCheck with success but no JSON Result", function () {
                spyOn(window, "setTimeout").andCallFake(function (a, int) {
                    if (int === 5000) {
                        a();
                    }
                });
                spyOn($, "ajax").andCallFake(function (opt) {
                    expect(opt.url).toEqual(
                        "https://www.arangodb.org/repositories/versions." +
                            "php?callback=parseVersions");
                    expect(opt.dataType).toEqual("jsonp");
                    expect(opt.crossDomain).toEqual(true);
                    expect(opt.async).toEqual(true);
                    opt.success("success");
                });
                spyOn(arangoHelper, "arangoNotification");
                r.initVersionCheck();
                expect(arangoHelper.arangoNotification).not.toHaveBeenCalled();


            });

            it("initVersionCheck with installed Version 2.0.1, latest 2.0.1, stablee 2.0.1",
                function () {
                    spyOn(window, "setTimeout").andCallFake(function (a, int) {
                        if (int === 5000) {
                            a();
                        }
                    });
                    spyOn($, "ajax").andCallFake(function (opt) {
                        expect(opt.url).toEqual(
                            "https://www.arangodb.org/repositories/versions." +
                                "php?callback=parseVersions");
                        expect(opt.dataType).toEqual("jsonp");
                        expect(opt.crossDomain).toEqual(true);
                        expect(opt.async).toEqual(true);
                        opt.success({
                                "1.4": {"stable": false, "versions": {
                                    "1.4.7": {"changes": "https:\/\/www.arangodb.org\/1.4.7"},
                                    "1.4.8": {"changes": "https:\/\/www.arangodb.org\/1.4.8"}}
                                },
                                "1.5": {"stable": false, "versions": {
                                    "1.5.0": {"changes": "https:\/\/www.arangodb.org\/1.5"},
                                    "1.5.1": {"changes": "https:\/\/www.arangodb.org\/1.5"}
                                }
                                },
                                "2.0": {"stable": true, "versions": {
                                    "2.0.0": {"changes": "https:\/\/www.arangodb.org\/2.0"},
                                    "2.0.1": {"changes": "https:\/\/www.arangodb.org\/2.0.1"}
                                }
                                }
                            }
                        );
                    });
                    footerDummy.system.version = "2.0.1";
                    spyOn(arangoHelper, "arangoNotification");
                    r.initVersionCheck();
                    expect(arangoHelper.arangoNotification).not.toHaveBeenCalled();
                });

            it("initVersionCheck with installed Version 1.4.5, " +
                "latest 2.0.1, stable 2.0.1", function () {
                spyOn(window, "setTimeout").andCallFake(function (a, int) {
                    if (int === 5000) {
                        a();
                    }
                });
                spyOn($, "ajax").andCallFake(function (opt) {
                    expect(opt.url).toEqual(
                        "https://www.arangodb.org/repositories/versions." +
                            "php?callback=parseVersions");
                    expect(opt.dataType).toEqual("jsonp");
                    expect(opt.crossDomain).toEqual(true);
                    expect(opt.async).toEqual(true);
                    opt.success({
                            "1.4": {stable: false,
                                versions: {
                                    "1.4.7": {changes: "https:\/\/www.arangodb.org\/1.4.7"},
                                    "1.4.8": {changes: "https:\/\/www.arangodb.org\/1.4.8"}
                                }
                            },
                            "1.5": {stable: false,
                                versions: {
                                    "1.5.0": {changes: "https:\/\/www.arangodb.org\/1.5"},
                                    "1.5.1": {changes: "https:\/\/www.arangodb.org\/1.5"}
                                }
                            },
                            "2.0": {stable: true,
                                versions: {
                                    "2.0.0": {changes: "https:\/\/www.arangodb.org\/2.0"},
                                    "2.0.1": {changes: "https:\/\/www.arangodb.org\/2.0.1"}
                                }
                            }
                        }
                    );
                });
                footerDummy.system.version = "1.4.7";
                spyOn(arangoHelper, "arangoNotification");
                r.initVersionCheck();
                var changes = "https:\/\/www.arangodb.org\/2.0.1";
                expect(arangoHelper.arangoNotification).toHaveBeenCalledWith(
                    "A newer version of ArangoDB (2.0.1" +
                        ") has become available. You may want to check the " +
                        "changelog at <a href=\"" + changes + "\">" +
                        changes + "</a>", 15000);
            });

            it("initVersionCheck with installed Version 2.0.0, latest 2.0.2, stable 2.0.1",
                function () {
                    spyOn(window, "setTimeout").andCallFake(function (a, int) {
                        if (int === 5000) {
                            a();
                        }
                    });
                    spyOn($, "ajax").andCallFake(function (opt) {
                        expect(opt.url).toEqual(
                            "https://www.arangodb.org/repositories/versions." +
                                "php?callback=parseVersions");
                        expect(opt.dataType).toEqual("jsonp");
                        expect(opt.crossDomain).toEqual(true);
                        expect(opt.async).toEqual(true);
                        opt.success({
                                "2.0": {stable: false,
                                    versions: {
                                        "2.0.0": {changes: "https:\/\/www.arangodb.org\/2.0.0"},
                                        "2.0.1": {changes: "https:\/\/www.arangodb.org\/2.0.1"}
                                    }
                                },
                                "2.1": {stable: true,
                                    versions: {
                                        "2.1.0": {changes: "https:\/\/www.arangodb.org\/2.1.0"},
                                        "2.1.1": {changes: "https:\/\/www.arangodb.org\/2.1.1"}
                                    }
                                }
                            }
                        );
                    });
                    footerDummy.system.version = "2.1.0";
                    spyOn(arangoHelper, "arangoNotification");
                    r.initVersionCheck();
                    var changes = "https:\/\/www.arangodb.org\/2.1.1";
                    expect(arangoHelper.arangoNotification).toHaveBeenCalledWith(
                        "A newer version of ArangoDB (2.1.1" +
                            ") has become available. You may want to check the " +
                            "changelog at <a href=\"" + changes + "\">" +
                            changes + "</a>", 15000);
                });


            it("initVersionCheck with error", function () {
                spyOn(window, "setTimeout").andCallFake(function (a, int) {
                    if (int === 5000) {
                        a();
                    } else {
                        expect(int).toEqual(60000);
                    }
                });
                spyOn($, "ajax").andCallFake(function (opt) {
                    expect(opt.url).toEqual(
                        "https://www.arangodb.org/repositories/versions." +
                            "php?callback=parseVersions");
                    expect(opt.dataType).toEqual("jsonp");
                    expect(opt.crossDomain).toEqual(true);
                    expect(opt.async).toEqual(true);
                    opt.error();
                });
                footerDummy.system.version = "2.1.0";
                spyOn(arangoHelper, "arangoNotification");
                r.initVersionCheck();
            });

        });

        describe("logsAllowed, checkUser, collections", function () {

            var r;


            beforeEach(function () {
                spyOn($, "ajax").andCallFake(function (opt) {
                    if (opt.url ==="https://www.arangodb.org/repositories/versions." +
                        "php?callback=parseVersions") {
                        opt.success("success");
                    }
                });
                r = new window.Router();
            });

            it("logsAllowed", function () {
                window.currentDB = new window.DatabaseModel({name: "_system"});
                expect(r.logsAllowed()).toEqual(true);
            });
            it("logs not Allowed", function () {
                window.currentDB = new window.DatabaseModel({name: "system"});
                expect(r.logsAllowed()).toEqual(false);
            });

            it("checkUser logged in", function () {
                sessionDummy.models = [1, 2];
                expect(r.checkUser()).toEqual(true);
            });
            it("checkUser not logged in", function () {
                spyOn(r, "navigate");
                sessionDummy.models = [];
                expect(r.checkUser()).toEqual(false);
                expect(r.navigate).toHaveBeenCalledWith("login", {trigger: true});
            });

            it("collections", function () {
                spyOn(window.collectionsView, "render");
                storeDummy.fetch = function (a) {
                    a.success();
                };
                r.collections();
                expect(window.collectionsView.render).toHaveBeenCalled();
                expect(naviDummy.selectMenuItem).toHaveBeenCalledWith('collections-menu');
            });
        });


    });
}());
