/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, runs, expect, waitsFor*/
/*global GraphManagementView, _, jasmine, $*/

(function () {
    "use strict";

    describe("Graph Management View", function () {

        var view,
          div,
          modalDiv,
          graphs,
          collections,
          e1, e2, e3,
          f1, f2, f3,
          t1, t2, t3,
          o1, o2, o3,
          sys1, cols;

        beforeEach(function () {
            modalDiv = document.createElement("div");
            modalDiv.id = "modalPlaceholder";
            document.body.appendChild(modalDiv);
            window.modalView = new window.ModalView();
            collections = new window.arangoCollections(cols);
            graphs = new window.GraphCollection();
            div = document.createElement("div");
            div.id = "content";
            document.body.appendChild(div);
            view = new window.GraphManagementView({
                collection: graphs,
                collectionCollection: new window.arangoCollections()
            });
        });

        afterEach(function () {
            document.body.removeChild(div);
            document.body.removeChild(modalDiv);
        });

        it("should fetch the graphs on render", function () {
            spyOn(graphs, "fetch");
            view.render();
            expect(graphs.fetch).toHaveBeenCalledWith({async: false});
        });

        describe("after rendering", function () {

            var g1, g2, g3;

            beforeEach(function () {
                g1 = {
                    _id: "_graphs/g1",
                    _key: "g1",
                    _rev: "123",
                    edgeDefinitions:
                      [
                        {
                          collection: e1,
                          from: ["f1"],
                          to: ["t1"]

                        }
                      ],
                    orphanCollections: ["o1"]
                };
                g2 = {
                    _id: "_graphs/g2",
                    _key: "g2",
                    _rev: "321",
                  edgeDefinitions:
                    [
                      {
                        collection: e2,
                        from: ["f2"],
                        to: ["t2"]

                      }
                    ],
                  orphanCollections: ["o2"]
                };
                g3 = {
                    _id: "_graphs/g3",
                    _key: "g3",
                    _rev: "111",
                  edgeDefinitions:
                    [
                      {
                        collection: e3,
                        from: ["f3"],
                        to: ["t3"]

                      }
                    ],
                  orphanCollections: ["o3"]
                };
                spyOn(graphs, "fetch");
                graphs.add(g1);
                graphs.add(g2);
                graphs.add(g3);
                view.render();
            });

            it("should create a sorted list of all graphs", function () {
                var list = $("div.tile h5.collectionName", "#graphManagementThumbnailsIn");
                expect(list.length).toEqual(3);
                // Order would be g2, g3, g1
                expect($(list[0]).html()).toEqual(g1._key);
                expect($(list[1]).html()).toEqual(g2._key);
                expect($(list[2]).html()).toEqual(g3._key);
            });


            it("should loadGraphViewer", function () {
              var e = {
                  currentTarget: {
                    id: "blabalblub"
                  }
                },
                a = {
                  attr: function (x) {
                    return "blabalblub";
                  },
                  width : function () {
                    return 100
                  },
                  html : function () {

                  }
                };
              spyOn(window, "GraphViewerUI");

              spyOn(window, "$").andReturn(a);


              view.loadGraphViewer(e);

              expect(window.GraphViewerUI).toHaveBeenCalledWith(
                undefined,
                {
                  type : 'gharial',
                  graphName : 'blaba',
                  baseUrl : '/_db/_system/'
                }, 25, 680,
                {
                  nodeShaper :
                  {
                    label : '_key',
                    color : { type : 'attribute', key : '_key' }
                  }
                }, true
              );
            });

          it("should delete graph", function () {
            var e = {
                currentTarget: {
                  id: "blabalblub"
                }
              },
              a = [{
                value: "blabalblub"
              }],collReturn = {
                destroy : function (a) {
                  a.success();
                }
              };

            spyOn(graphs, "get").andReturn(collReturn);

            spyOn(window, "$").andReturn(a);

            spyOn(window.modalView, "hide").andReturn(a);

            view.deleteGraph(e);

            expect(graphs.get).toHaveBeenCalledWith("blabalblub");
            expect(window.modalView.hide).toHaveBeenCalled();
          });

          it("should NOT delete graph", function () {
            var e = {
                currentTarget: {
                  id: "blabalblub"
                }
              },
              a = [{
                value: "blabalblub"
              }],collReturn = {
                destroy : function (a) {
                  a.error("", {responseText : '{"errorMessage" : "errorMessage"}'});
                }
              };

            spyOn(graphs, "get").andReturn(collReturn);

            spyOn(window, "$").andReturn(a);
            spyOn(arangoHelper, "arangoError");

            spyOn(window.modalView, "hide").andReturn(a);

            view.deleteGraph(e);

            expect(graphs.get).toHaveBeenCalledWith("blabalblub");
            expect(arangoHelper.arangoError).toHaveBeenCalledWith("errorMessage");
            expect(window.modalView.hide).toHaveBeenCalled();


          });

          it("should set to and from for added definition", function () {
            view.removedECollList = [];
            var model = view.collection.create({
              _key: "blub",
              name: "blub",
              edgeDefinitions: [{
                collection: "blub",
                from: ["bla"],
                to: ["blob"]
              }],
              orphanCollections: []
            });
            var e = {
                currentTarget: {
                  id: "blabalblub"
                },
                stopPropagation : function () {},
                added : {
                  id : "moppel"

                },
                val : "newEdgeDefintion"
              },
              a = {
                select2: function () {},
                attr: function () {},
                length : 2,
                0 : {id : 1},
                1 : {id : 2}
              },collReturn = {
                destroy : function (a) {
                  a.error("", {responseText : '{"errorMessage" : "errorMessage"}'});
                }
              };

            spyOn(e, "stopPropagation");

            spyOn(window, "$").andReturn(a);

            spyOn(window.modalView, "hide").andReturn(a);

            view.setFromAndTo(e);

            expect(e.stopPropagation).toHaveBeenCalled();

            model.destroy();


          });


          it("should set to and from for added definition for already known def", function () {
            view.removedECollList = [];
            var model = view.collection.create({
              _key: "blub2",
              name: "blub2",
              edgeDefinitions: [{
                collection: "blub",
                from: ["bla"],
                to: ["blob"]
              }],
              orphanCollections: []
            });
            var e = {
                currentTarget: {
                  id: "blabalblub"
                },
                stopPropagation : function () {},
                added : {
                  id : "moppel"

                },
                val : "blub"
              },
              a = {
                select2: function () {},
                attr: function () {},
                length : 2,
                0 : {id : 1},
                1 : {id : 2}
              },collReturn = {
                destroy : function (a) {
                  a.error("", {responseText : '{"errorMessage" : "errorMessage"}'});
                }
              };

            spyOn(e, "stopPropagation");

            spyOn(window, "$").andReturn(a);

            spyOn(window.modalView, "hide").andReturn(a);

            view.setFromAndTo(e);

            expect(e.stopPropagation).toHaveBeenCalled();

            model.destroy();


          });

          it("should not set to and from for added definition as already in use and has been entered manually", function () {
            view.removedECollList = ["moppel"];
            var model = view.collection.create({
              _key: "blub2",
              name: "blub2",
              edgeDefinitions: [{
                collection: "blub",
                from: ["bla"],
                to: ["blob"]
              }],
              orphanCollections: []
            });
            var e = {
                currentTarget: {
                  id: "blabalblub"
                },
                stopPropagation : function () {},
                added : {
                  id : "moppel"

                },
                val : "blub"
              },
              a = {
                select2: function () {},
                attr: function () {},
                length : 2,
                0 : {id : 1},
                1 : {id : 2}
              },collReturn = {
                destroy : function (a) {
                  a.error("", {responseText : '{"errorMessage" : "errorMessage"}'});
                }
              };

            spyOn(e, "stopPropagation");

            spyOn(window, "$").andReturn(a);

            spyOn(window.modalView, "hide").andReturn(a);

            view.setFromAndTo(e);

            expect(e.stopPropagation).toHaveBeenCalled();

            model.destroy();


          });

          it("should not set to and from for removed definition as already in use and has been entered manually", function () {
            view.removedECollList = ["moppel"];
            var model = view.collection.create({
              _key: "blub2",
              name: "blub2",
              edgeDefinitions: [{
                collection: "blub",
                from: ["bla"],
                to: ["blob"]
              }],
              orphanCollections: []
            });
            var e = {
                currentTarget: {
                  id: "blabalblub"
                },
                stopPropagation : function () {},
                val : "blub",
                removed : {id : "moppel"}
              },
              a = {
                select2: function () {},
                attr: function () {},
                length : 2,
                0 : {id : 1},
                1 : {id : 2}
              },collReturn = {
                destroy : function (a) {
                  a.error("", {responseText : '{"errorMessage" : "errorMessage"}'});
                }
              };

            spyOn(e, "stopPropagation");

            spyOn(window, "$").andReturn(a);

            spyOn(window.modalView, "hide").andReturn(a);

            view.setFromAndTo(e);

            expect(e.stopPropagation).toHaveBeenCalled();
            expect(view.removedECollList.indexOf("moppel")).toEqual(-1)
            expect(view.eCollList.indexOf("moppel")).not.toEqual(-1)
            model.destroy();


          });



          /*setFromAndTo : function (e) {
            e.stopPropagation();
            var map = this.calculateEdgeDefinitionMap(), id, i, tmp;

            if (e.added) {
              if (this.eCollList.indexOf(e.added.id) === -1 &&
                this.removedECollList.indexOf(e.added.id) !== -1) {
                id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
                $('input[id*="newEdgeDefinitions' + id  + '"]').select2("val", null);
                $('input[id*="newEdgeDefinitions' + id  + '"]').attr(
                  "placeholder","The collection "+ e.added.id + " is already used."
                );
                return;
              }
              this.removedECollList.push(e.added.id);
              this.eCollList.splice(this.eCollList.indexOf(e.added.id),1);
            } else {
              this.eCollList.push(e.removed.id);
              this.removedECollList.splice(this.removedECollList.indexOf(e.removed.id),1);
            }

            if (map[e.val]) {
              id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
              $('#s2id_fromCollections'+id).select2("val", map[e.val].from);
              $('#fromCollections'+id).attr('disabled', true);
              $('#s2id_toCollections'+id).select2("val", map[e.val].to);
              $('#toCollections'+id).attr('disabled', true);
            } else {
              id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
              $('#s2id_fromCollections'+id).select2("val", null);
              $('#fromCollections'+id).attr('disabled', false);
              $('#s2id_toCollections'+id).select2("val", null);
              $('#toCollections'+id).attr('disabled', false);
            }
            tmp = $('input[id*="newEdgeDefinitions"]');
            for (i = 0; i < tmp.length ; i++) {
              id = tmp[i].id;
              $('#' + id).select2({
                tags : this.eCollList,
                showSearchBox: false,
                minimumResultsForSearch: -1,
                width: "336px",
                maximumSelectionSize: 1
              });
            }

          },*/

          describe("creating a new graph", function () {

                it("should create a new empty graph", function () {
                    runs(function () {
                        $("#createGraph").click();
                    });
                    waitsFor(function () {
                        return $("#modal-dialog").css("display") === "block";
                    });
                    runs(function () {
                        $("#createNewGraphName").val("newGraph");
                        $("#s2id_newEdgeDefinitions0").val(["newEdgeCol"]);
                        $("#newGraphEdges").val("newEdges");
                        spyOn($, "ajax").andCallFake(function (opts) {
                            expect(opts.type).toEqual("POST");
                            expect(opts.url).toEqual("/_api/gharial");
                            expect(opts.data).toEqual(JSON.stringify(
                              {
                                "name":"newGraph",
                                "edgeDefinitions":[],
                                "orphanCollections":[]
                              }));
                        });
                        $("#modalButton1").click();
                        expect($.ajax).toHaveBeenCalled();
                    });
                });

            });

        });

    });

}());

