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
          edgeDefinitions: [
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
          edgeDefinitions: [
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
          edgeDefinitions: [
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
            $("#s2id_newEdgeDefinitions0").select2("val", ["newEdgeCol"]);
            $("#s2id_fromCollections0").select2("val", ["newFrom1", "newFrom2"]);
            $("#s2id_toCollections0").select2("val", ["newTo1", "newTo2"]);
            $("#s2id_newVertexCollections").select2("val", ["newOrphan1", "newOrphan2"]);
            $("#newGraphEdges").val("newEdges");
            spyOn($, "ajax").andCallFake(function (opts) {
              expect(opts.type).toEqual("POST");
              expect(opts.url).toEqual("/_api/gharial");
              expect(opts.data).toEqual(JSON.stringify(
                {
                  "name": "newGraph",
                  "edgeDefinitions": [
                    {
                      collection: "newEdgeCol",
                      from: ["newFrom1", "newFrom2"],
                      to: ["newTo1", "newTo2"]
                    }
                  ],
                  "orphanCollections": ["newOrphan1", "newOrphan2"]
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

