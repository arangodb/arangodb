/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The documents view", function() {

    var view, jQueryDummy;

    beforeEach(function() {
        view = new window.DocumentsView();
    });

    it("assert the basics", function() {
        expect(view.collectionID).toEqual(0);
        expect(view.currentPage).toEqual(1);
        expect(view.documentsPerPage).toEqual(10);
        expect(view.totalPages).toEqual(1);
        expect(view.filters).toEqual({ "0" : true });
        expect(view.filterId).toEqual(0);
        expect(view.addDocumentSwitch).toEqual(true);
        expect(view.allowUpload).toEqual(false);
        expect(view.collectionContext).toEqual({
            prev: null,
            next: null
        });
        expect(view.alreadyClicked).toEqual(false);
        expect(view.table).toEqual('#documentsTableID');
        expect(view.events).toEqual({
            "click #collectionPrev"      : "prevCollection",
            "click #collectionNext"      : "nextCollection",
            "click #filterCollection"    : "filterCollection",
            "click #indexCollection"     : "indexCollection",
            "click #importCollection"    : "importCollection",
            "click #filterSend"          : "sendFilter",
            "click #addFilterItem"       : "addFilterItem",
            "click .removeFilterItem"    : "removeFilterItem",
            "click #confirmCreateEdge"   : "addEdge",
            "click #documentsTableID tr" : "clicked",
            "click #deleteDoc"           : "remove",
            "click #addDocumentButton"   : "addDocument",
            "click #documents_first"     : "firstDocuments",
            "click #documents_last"      : "lastDocuments",
            "click #documents_prev"      : "prevDocuments",
            "click #documents_next"      : "nextDocuments",
            "click #confirmDeleteBtn"    : "confirmDelete",
            "keyup #createEdge"          : "listenKey",
            "click .key"                 : "nop",
            "keyup"                      : "returnPressedHandler",
            "keydown .filterValue"       : "filterValueKeydown",
            "click #importModal"         : "showImportModal",
            "click #resetView"           : "resetView",
            "click #confirmDocImport"    : "startUpload",
            "change #newIndexType"       : "selectIndexType",
            "click #createIndex"         : "createIndex",
            "click .deleteIndex"         : "prepDeleteIndex",
            "click #confirmDeleteIndexBtn"    : "deleteIndex",
            "click #documentsToolbar ul"      : "resetIndexForms",
            "click #indexHeader #addIndex"    :    "toggleNewIndexView",
            "click #indexHeader #cancelIndex" :    "toggleNewIndexView"
        });
    });


      it("showSpinner", function() {
          jQueryDummy = {
              show : function() {}
          }
          spyOn(jQueryDummy, "show");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.showSpinner();
          expect(window.$).toHaveBeenCalledWith("#uploadIndicator");
          expect(jQueryDummy.show).toHaveBeenCalled();
      });

      it("hideSpinner", function() {
          jQueryDummy = {
              hide : function() {}
          }
          spyOn(jQueryDummy, "hide");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.hideSpinner();
          expect(window.$).toHaveBeenCalledWith("#uploadIndicator");
          expect(jQueryDummy.hide).toHaveBeenCalled();
      });


      it("showImportModal", function() {
          jQueryDummy = {
              modal : function() {}
          }
          spyOn(jQueryDummy, "modal");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.showImportModal();
          expect(window.$).toHaveBeenCalledWith("#docImportModal");
          expect(jQueryDummy.modal).toHaveBeenCalledWith("show");
      });


      it("hideImportModal", function() {
          jQueryDummy = {
              modal : function() {}
          }
          spyOn(jQueryDummy, "modal");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.hideImportModal();
          expect(window.$).toHaveBeenCalledWith("#docImportModal");
          expect(jQueryDummy.modal).toHaveBeenCalledWith("hide");
      });


      it("returnPressedHandler call confirmDelete if keyCode == 13", function() {
          jQueryDummy = {
              attr : function() {return false;}
          }
          spyOn(jQueryDummy, "attr").andCallThrough();
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          spyOn(view, "confirmDelete").andCallFake(function () {});
          view.returnPressedHandler({keyCode : 13});
          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled");
          expect(view.confirmDelete).toHaveBeenCalled();
      });

      it("returnPressedHandler does not call confirmDelete if keyCode !== 13", function() {
          jQueryDummy = {
              attr : function() {return false;}
          }
          spyOn(jQueryDummy, "attr");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          spyOn(view, "confirmDelete").andCallFake(function () {});
          view.returnPressedHandler({keyCode : 11});
          expect(jQueryDummy.attr).not.toHaveBeenCalled();
          expect(view.confirmDelete).not.toHaveBeenCalled();
      });

      it("returnPressedHandler does not call confirmDelete if keyCode == 13 but" +
          " confirmButton is disabled", function() {
          jQueryDummy = {
              attr : function() {return true;}
          }
          spyOn(jQueryDummy, "attr");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          spyOn(view, "confirmDelete").andCallFake(function () {});
          view.returnPressedHandler({keyCode : 13});
          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled");
          expect(view.confirmDelete).not.toHaveBeenCalled();
      });

      it("toggleNewIndexView", function() {
          jQueryDummy = {
              toggle : function() {}
          }
          spyOn(jQueryDummy, "toggle");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          spyOn(view, "resetIndexForms").andCallFake(function () {});
          view.toggleNewIndexView();
          expect(window.$).toHaveBeenCalledWith("#indexEditView");
          expect(window.$).toHaveBeenCalledWith("#newIndexView");
          expect(jQueryDummy.toggle).toHaveBeenCalledWith("fast");
          expect(view.resetIndexForms).toHaveBeenCalled();
      });


      it("nop", function() {
          var event = {stopPropagation : function () {}};
          spyOn(event, "stopPropagation");
          view.nop(event);
          expect(event.stopPropagation).toHaveBeenCalled();
      });

      it("listenKey with keyCode 13", function() {
          spyOn(view, "addEdge");
          view.listenKey({keyCode : 13});
          expect(view.addEdge).toHaveBeenCalled();
      });

      it("listenKey with keyCode != 13", function() {
          spyOn(view, "addEdge");
          view.listenKey({keyCode : 12});
          expect(view.addEdge).not.toHaveBeenCalled();
      });


      it("resetView", function() {
          jQueryDummy = {
              val : function() {},
              css : function() {}
          }
          spyOn(jQueryDummy, "val");
          spyOn(jQueryDummy, "css");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          spyOn(view, "removeAllFilterItems").andCallFake(function () {});
          spyOn(view, "clearTable").andCallFake(function () {});
          var arangoDocStoreDummy = {
              getDocuments : function() {}
          };
          spyOn(arangoDocStoreDummy, "getDocuments");
          spyOn(window, "arangoDocuments").andReturn(arangoDocStoreDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();
          view.resetView();
          expect(window.$).toHaveBeenCalledWith("input");
          expect(window.$).toHaveBeenCalledWith("select");
          expect(window.$).toHaveBeenCalledWith("#documents_last");
          expect(window.$).toHaveBeenCalledWith("#documents_first");
          expect(jQueryDummy.val).toHaveBeenCalledWith('');
          expect(jQueryDummy.val).toHaveBeenCalledWith('==');
          expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "visible");
          expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "visible");
          expect(arangoDocStoreDummy.getDocuments).toHaveBeenCalledWith(view.collectionID, 1);
          expect(view.removeAllFilterItems).toHaveBeenCalled();
          expect(view.clearTable).toHaveBeenCalled();
          expect(view.addDocumentSwitch).toEqual(true);
      });


      it("start succesful Upload mit XHR ready state = 4, XHR status = 201 and parseable JSON", function() {
          spyOn(view, "showSpinner");
          spyOn(view, "hideSpinner");
          spyOn(view, "hideImportModal");
          spyOn(view, "resetView");
          spyOn($, "ajax").andCallFake(function (opt) {
              expect(opt.url).toEqual('/_api/import?type=auto&collection='+
                  encodeURIComponent(view.colid)+
                  '&createCollection=false');
              expect(opt.dataType).toEqual("json");
              expect(opt.contentType).toEqual("json");
              expect(opt.processData).toEqual(false);
              expect(opt.data).toEqual(view.file);
              expect(opt.async).toEqual(false);
              expect(opt.type).toEqual("POST");
              opt.complete({readyState : 4, status  : 201, responseText : '{"a" : 1}'});
          });

          view.allowUpload = true;

          view.startUpload();
          expect(view.showSpinner).toHaveBeenCalled();
          expect(view.hideSpinner).toHaveBeenCalled();
          expect(view.hideImportModal).toHaveBeenCalled();
          expect(view.resetView).toHaveBeenCalled();
      });

      it("start succesful Upload mit XHR ready state != 4", function() {
          spyOn(view, "showSpinner");
          spyOn(view, "hideSpinner");
          spyOn(view, "hideImportModal");
          spyOn(view, "resetView");
          spyOn($, "ajax").andCallFake(function (opt) {
              expect(opt.url).toEqual('/_api/import?type=auto&collection='+
                  encodeURIComponent(view.colid)+
                  '&createCollection=false');
              expect(opt.dataType).toEqual("json");
              expect(opt.contentType).toEqual("json");
              expect(opt.processData).toEqual(false);
              expect(opt.data).toEqual(view.file);
              expect(opt.async).toEqual(false);
              expect(opt.type).toEqual("POST");
              opt.complete({readyState : 3, status  : 201, responseText : '{"a" : 1}'});
          });

          view.allowUpload = true;
          spyOn(arangoHelper, "arangoError");
          view.startUpload();
          expect(view.showSpinner).toHaveBeenCalled();
          expect(view.hideSpinner).toHaveBeenCalled();
          expect(view.hideImportModal).not.toHaveBeenCalled();
          expect(view.resetView).not.toHaveBeenCalled();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith("Upload error");
      });

      it("start succesful Upload mit XHR ready state = 4, XHR status = 201 and not parseable JSON", function() {
          spyOn(view, "showSpinner");
          spyOn(view, "hideSpinner");
          spyOn(view, "hideImportModal");
          spyOn(view, "resetView");
          spyOn($, "ajax").andCallFake(function (opt) {
              expect(opt.url).toEqual('/_api/import?type=auto&collection='+
                  encodeURIComponent(view.colid)+
                  '&createCollection=false');
              expect(opt.dataType).toEqual("json");
              expect(opt.contentType).toEqual("json");
              expect(opt.processData).toEqual(false);
              expect(opt.data).toEqual(view.file);
              expect(opt.async).toEqual(false);
              expect(opt.type).toEqual("POST");
              opt.complete({readyState : 4, status  : 201, responseText : "blub"});
          });

          view.allowUpload = true;
          spyOn(arangoHelper, "arangoError");
          view.startUpload();
          expect(view.showSpinner).toHaveBeenCalled();
          expect(view.hideSpinner).toHaveBeenCalled();
          expect(view.hideImportModal).toHaveBeenCalled();
          expect(view.resetView).toHaveBeenCalled();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Error: SyntaxError: Unable to parse JSON string');
      });


      it("uploadSetup", function() {
          jQueryDummy = {
              change : function(e) {
                  e({target : {files : ["BLUB"]}});
              }
          }
          spyOn(jQueryDummy, "change").andCallThrough();
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.uploadSetup();
          expect(window.$).toHaveBeenCalledWith("#importDocuments");
          expect(jQueryDummy.change).toHaveBeenCalledWith(jasmine.any(Function));
          expect(view.files).toEqual(["BLUB"]);
          expect(view.file).toEqual("BLUB");
          expect(view.allowUpload).toEqual(true);
      });

      it("buildCollectionLink", function() {
          expect(view.buildCollectionLink({get: function() {return "blub"}})).toEqual(
              "collection/" + encodeURIComponent("blub") + '/documents/1'
          );
      });


      it("prevCollection with no collectionContext.prev", function() {
          jQueryDummy = {
              parent : function(e) {
                  return jQueryDummy;
              },
              addClass : function() {

              }

          }
          spyOn(jQueryDummy, "parent").andCallThrough();;
          spyOn(jQueryDummy, "addClass");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.collectionContext = {prev : null};
          view.prevCollection();
          expect(window.$).toHaveBeenCalledWith("#collectionPrev");
          expect(jQueryDummy.parent).toHaveBeenCalled();
          expect(jQueryDummy.addClass).toHaveBeenCalledWith("disabledPag");
      });

      it("prevCollection with collectionContext.prev", function() {
          jQueryDummy = {
              parent : function(e) {
                  return jQueryDummy;
              },
              removeClass : function() {

              }

          }
          spyOn(jQueryDummy, "parent").andCallThrough();;
          spyOn(jQueryDummy, "removeClass");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.collectionContext = {prev : 1};
          spyOn(window.App, "navigate");
          spyOn(view, "buildCollectionLink").andReturn(1);
          view.prevCollection();
          expect(window.$).toHaveBeenCalledWith("#collectionPrev");
          expect(jQueryDummy.parent).toHaveBeenCalled();
          expect(view.buildCollectionLink).toHaveBeenCalledWith(1);
          expect(window.App.navigate).toHaveBeenCalledWith(1, {
              trigger: true
          });
          expect(jQueryDummy.removeClass).toHaveBeenCalledWith("disabledPag");
      });


      it("nextCollection with no collectionContext.next", function() {
          jQueryDummy = {
              parent : function(e) {
                  return jQueryDummy;
              },
              addClass : function() {

              }

          }
          spyOn(jQueryDummy, "parent").andCallThrough();;
          spyOn(jQueryDummy, "addClass");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.collectionContext = {next : null};
          view.nextCollection();
          expect(window.$).toHaveBeenCalledWith("#collectionNext");
          expect(jQueryDummy.parent).toHaveBeenCalled();
          expect(jQueryDummy.addClass).toHaveBeenCalledWith("disabledPag");
      });

      it("nextCollection with collectionContext.next", function() {
          jQueryDummy = {
              parent : function(e) {
                  return jQueryDummy;
              },
              removeClass : function() {

              }

          }
          spyOn(jQueryDummy, "parent").andCallThrough();;
          spyOn(jQueryDummy, "removeClass");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.collectionContext = {next : 1};
          spyOn(window.App, "navigate");
          spyOn(view, "buildCollectionLink").andReturn(1);
          view.nextCollection();
          expect(window.$).toHaveBeenCalledWith("#collectionNext");
          expect(jQueryDummy.parent).toHaveBeenCalled();
          expect(view.buildCollectionLink).toHaveBeenCalledWith(1);
          expect(window.App.navigate).toHaveBeenCalledWith(1, {
              trigger: true
          });
          expect(jQueryDummy.removeClass).toHaveBeenCalledWith("disabledPag");
      });


      it("filterCollection", function() {
          jQueryDummy = {
              removeClass : function() {
              },
              toggleClass : function() {
              },
              slideToggle : function() {
              },
              hide : function() {
              },
              focus : function() {
              }

          }
          spyOn(jQueryDummy, "removeClass");
          spyOn(jQueryDummy, "toggleClass");
          spyOn(jQueryDummy, "slideToggle");
          spyOn(jQueryDummy, "hide");
          spyOn(jQueryDummy, "focus");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.filters = [{0 : "bla"}, {1 : "blub"}];
          view.filterCollection();
          expect(window.$).toHaveBeenCalledWith("#indexCollection");
          expect(window.$).toHaveBeenCalledWith("#importCollection");
          expect(window.$).toHaveBeenCalledWith("#filterCollection");
          expect(window.$).toHaveBeenCalledWith("#filterHeader");
          expect(window.$).toHaveBeenCalledWith("#importHeader");
          expect(window.$).toHaveBeenCalledWith("#indexHeader");
          expect(window.$).toHaveBeenCalledWith("#attribute_name0");
          // next assertion seems strange but follows the strange implementation
          expect(window.$).not.toHaveBeenCalledWith("#attribute_name1");
          expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
          expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
          expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
          expect(jQueryDummy.hide).toHaveBeenCalled();
          expect(jQueryDummy.focus).toHaveBeenCalled();
      });

      it("importCollection", function() {
          jQueryDummy = {
              removeClass : function() {
              },
              toggleClass : function() {
              },
              slideToggle : function() {
              },
              hide : function() {
              }

          }
          spyOn(jQueryDummy, "removeClass");
          spyOn(jQueryDummy, "toggleClass");
          spyOn(jQueryDummy, "slideToggle");
          spyOn(jQueryDummy, "hide");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.importCollection();

          expect(window.$).toHaveBeenCalledWith("#filterCollection");
          expect(window.$).toHaveBeenCalledWith("#indexCollection");
          expect(window.$).toHaveBeenCalledWith("#importCollection");
          expect(window.$).toHaveBeenCalledWith("#filterHeader");
          expect(window.$).toHaveBeenCalledWith("#importHeader");
          expect(window.$).toHaveBeenCalledWith("#indexHeader");
          expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
          expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
          expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
          expect(jQueryDummy.hide).toHaveBeenCalled();
      });

      it("indexCollection", function() {
          jQueryDummy = {
              removeClass : function() {
              },
              toggleClass : function() {
              },
              slideToggle : function() {
              },
              hide : function() {
              },
              show : function() {
              }

          }
          spyOn(jQueryDummy, "removeClass");
          spyOn(jQueryDummy, "toggleClass");
          spyOn(jQueryDummy, "slideToggle");
          spyOn(jQueryDummy, "hide");
          spyOn(jQueryDummy, "show");
          spyOn(window, "$").andReturn(
              jQueryDummy
          );
          view.indexCollection();
          expect(window.$).toHaveBeenCalledWith("#filterCollection");
          expect(window.$).toHaveBeenCalledWith("#importCollection");
          expect(window.$).toHaveBeenCalledWith("#indexCollection");
          expect(window.$).toHaveBeenCalledWith("#newIndexView");
          expect(window.$).toHaveBeenCalledWith("#indexEditView");
          expect(window.$).toHaveBeenCalledWith("#indexHeader");
          expect(window.$).toHaveBeenCalledWith("#importHeader");
          expect(window.$).toHaveBeenCalledWith("#filterHeader");
          expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
          expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
          expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
          expect(jQueryDummy.hide).toHaveBeenCalled();
          expect(jQueryDummy.show).toHaveBeenCalled();
      });


      it("getFilterContent", function() {

          jQueryDummy = {
              e : undefined,
              val : function() {
                  var e = jQueryDummy.e;
                  if (e === '#attribute_value0') {
                    return '{"jsonval" : 1}';
                  } else if (e === '#attribute_value1') {
                    return "stringval";
                  } else if (e === '#attribute_name0') {
                    return 'name0';
                  } else if (e === '#attribute_name1') {
                    return 'name1';
                  } else if (e === '#operator0') {
                    return 'operator0';
                  }  else if (e === '#operator1') {
                      return 'operator1';
                  }
              }
          };
          spyOn(jQueryDummy, "val").andCallThrough();
          spyOn(window, "$").andCallFake(
              function (e) {
                  jQueryDummy.e = e;
                  return jQueryDummy;
              }
          );
          view.filters = [{0 : "bla"}, {1 : "blub"}];
          expect(view.getFilterContent()).toEqual([
              [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
              { param0 : {jsonval : 1}, param1 : "stringval"}
          ]);
          expect(window.$).toHaveBeenCalledWith("#attribute_value0");
          expect(window.$).toHaveBeenCalledWith("#attribute_value1");
          expect(window.$).toHaveBeenCalledWith("#attribute_name0");
          expect(window.$).toHaveBeenCalledWith("#attribute_name1");
          expect(window.$).toHaveBeenCalledWith("#operator0");
          expect(window.$).toHaveBeenCalledWith("#operator1");
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(jQueryDummy.val).toHaveBeenCalled();
      });


      it("sendFilter", function() {

          jQueryDummy = {
              css : function() {}

          };
          spyOn(jQueryDummy, "css");
          spyOn(window, "$").andReturn(jQueryDummy);
          spyOn(view, "getFilterContent").andReturn(
              [
                  [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                  { param0 : {jsonval : 1}, param1 : "stringval"}
              ]

          );

          var arangoDocStoreDummy = {
              getFilteredDocuments : function() {}
          };
          var documentsViewDummy = {
              clearTable : function() {}
          };
          spyOn(arangoDocStoreDummy, "getFilteredDocuments");
          spyOn(documentsViewDummy, "clearTable");
          spyOn(window, "arangoDocuments").andReturn(arangoDocStoreDummy);
          spyOn(window, "DocumentsView").andReturn(documentsViewDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();
          window.documentsView = new window.DocumentsView();


          view.sendFilter();

          expect(view.addDocumentSwitch).toEqual(false);
          expect(documentsViewDummy.clearTable).toHaveBeenCalled();
          expect(arangoDocStoreDummy.getFilteredDocuments).toHaveBeenCalledWith(
              view.colid, 1,
              [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
              { param0 : {jsonval : 1}, param1 : "stringval"}
          );


          expect(window.$).toHaveBeenCalledWith("#documents_last");
          expect(window.$).toHaveBeenCalledWith("#documents_first");
          expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "hidden");
          expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "hidden");
      });

      it("addFilterItem", function() {

          jQueryDummy = {
              append : function() {}

          };
          spyOn(jQueryDummy, "append");
          spyOn(window, "$").andReturn(jQueryDummy);

          view.filterId = 1;
          var num = 2;
          view.addFilterItem();
          expect(window.$).toHaveBeenCalledWith("#filterHeader");
          expect(jQueryDummy.append).toHaveBeenCalledWith(' <div class="queryline querylineAdd">'+
              '<input id="attribute_name' + num +
              '" type="text" placeholder="Attribute name">'+
              '<select name="operator" id="operator' +
              num + '" class="filterSelect">'+
              '    <option value="==">==</option>'+
              '    <option value="!=">!=</option>'+
              '    <option value="&lt;">&lt;</option>'+
              '    <option value="&lt;=">&lt;=</option>'+
              '    <option value="&gt;=">&gt;=</option>'+
              '    <option value="&gt;">&gt;</option>'+
              '</select>'+
              '<input id="attribute_value' + num +
              '" type="text" placeholder="Attribute value" ' +
              'class="filterValue">'+
              ' <a class="removeFilterItem" id="removeFilter' + num + '">' +
              '<i class="icon icon-minus arangoicon"></i></a></div>');
          expect(view.filters[num]).toEqual(true);

      });


      it("filterValueKeydown with keyCode == 13", function() {
          spyOn(view, "sendFilter");
          view.filterValueKeydown({keyCode : 13});
          expect(view.sendFilter).toHaveBeenCalled();
      });

      it("filterValueKeydown with keyCode !== 13", function() {
          spyOn(view, "sendFilter");
          view.filterValueKeydown({keyCode : 11});
          expect(view.sendFilter).not.toHaveBeenCalled();
      });


      it("removeFilterItem with keyCode !== 13", function() {
          jQueryDummy = {
              remove : function() {}

          };
          spyOn(jQueryDummy, "remove");
          spyOn(window, "$").andReturn(jQueryDummy);
          view.filters[1] = "bla";

          view.removeFilterItem({currentTarget : {id : "removeFilter1", parentElement : "#blub"}});
          expect(window.$).toHaveBeenCalledWith("#blub");
          expect(jQueryDummy.remove).toHaveBeenCalled();
          expect(view.filters[1]).toEqual(undefined);
      });

      it("removeAllFilterItems", function() {
          jQueryDummy = {
              children : function() {},
              parent : function() {}

          };
          spyOn(jQueryDummy, "children").andReturn([1, 2, 3]);
          spyOn(jQueryDummy, "parent").andReturn({remove : function () {}});
          spyOn(window, "$").andReturn(jQueryDummy);

          view.removeAllFilterItems();
          expect(window.$).toHaveBeenCalledWith("#filterHeader");
          expect(window.$).toHaveBeenCalledWith("#removeFilter1");
          expect(window.$).toHaveBeenCalledWith("#removeFilter2");
          expect(window.$).toHaveBeenCalledWith("#removeFilter3");
          expect(jQueryDummy.children).toHaveBeenCalled();
          expect(jQueryDummy.parent).toHaveBeenCalled();

          expect(view.filters).toEqual({ "0" : true });
          expect(view.filterId).toEqual(0);
      });

      it("addDocument without an error", function() {
          var arangoDocStoreDummy = {
              createTypeDocument : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeDocument").andReturn("newDoc");
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          spyOn(arangoHelper, "collectionApiType").andReturn("document");
          window.location.hash = "1/2";
          view.addDocument();

          expect(arangoHelper.collectionApiType).toHaveBeenCalledWith("2", true);
          expect(arangoDocStoreDummy.createTypeDocument).toHaveBeenCalledWith("2");
          expect(window.location.hash).toEqual("#collection/" + "newDoc");

      });

      it("addDocument with an edge", function() {
          var arangoDocStoreDummy = {
              createTypeDocument : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeDocument").andReturn("newDoc");
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          jQueryDummy = {
              modal : function() {}

          };
          spyOn(jQueryDummy, "modal");
          spyOn(arangoHelper, "fixTooltips");
          spyOn(window, "$").andReturn(jQueryDummy);

          spyOn(arangoHelper, "collectionApiType").andReturn("edge");
          window.location.hash = "1/2";
          view.addDocument();

          expect(window.$).toHaveBeenCalledWith("#edgeCreateModal");
          expect(jQueryDummy.modal).toHaveBeenCalledWith('show');
          expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(".modalTooltips", "left");
          expect(arangoHelper.collectionApiType).toHaveBeenCalledWith("2", true);
          expect(arangoDocStoreDummy.createTypeDocument).not.toHaveBeenCalled();
          expect(window.location.hash).toEqual("#1/2");

      });

      it("addDocument with an error", function() {
          var arangoDocStoreDummy = {
              createTypeDocument : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeDocument").andReturn(false);
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          spyOn(arangoHelper, "collectionApiType").andReturn("document");
          spyOn(arangoHelper, "arangoError");
          window.location.hash = "1/2";
          view.addDocument();

          expect(arangoHelper.collectionApiType).toHaveBeenCalledWith("2", true);
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Creating document failed');
          expect(arangoDocStoreDummy.createTypeDocument).toHaveBeenCalledWith("2");
          expect(window.location.hash).toEqual("#1/2");

      });


      it("addEdge with missing to ", function() {
          jQueryDummy = {
              e : undefined,
              val : function() {
                  if (jQueryDummy.e === "#new-document-from") {
                      return "";
                  };
                  if (jQueryDummy.e === "#new-document-to") {
                      return "bla";
                  }

              }

          };
          spyOn(jQueryDummy, "val").andCallThrough();
          spyOn(window, "$").andCallFake(function (e) {
              jQueryDummy.e = e;
              return jQueryDummy;

          });

          var arangoDocStoreDummy = {
              createTypeEdge : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeEdge");
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          window.location.hash = "1/2";
          view.addEdge();
          expect(window.$).toHaveBeenCalledWith("#new-document-from");
          expect(window.$).toHaveBeenCalledWith("#new-document-to");
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(arangoDocStoreDummy.createTypeEdge).not.toHaveBeenCalled();

      });


      it("addEdge with missing to ", function() {
          jQueryDummy = {
              e : undefined,
              val : function() {
                  if (jQueryDummy.e === "#new-document-from") {
                      return "bla";
                  };
                  if (jQueryDummy.e === "#new-document-to") {
                      return "";
                  }

              }

          };
          spyOn(jQueryDummy, "val").andCallThrough();
          spyOn(window, "$").andCallFake(function (e) {
              jQueryDummy.e = e;
              return jQueryDummy;

          });

          var arangoDocStoreDummy = {
              createTypeEdge : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeEdge");
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          window.location.hash = "1/2";
          view.addEdge();
          expect(window.$).toHaveBeenCalledWith("#new-document-from");
          expect(window.$).toHaveBeenCalledWith("#new-document-to");
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(arangoDocStoreDummy.createTypeEdge).not.toHaveBeenCalled();

      });



      it("addEdge with success", function() {
          jQueryDummy = {
              e : undefined,
              val : function() {
                  if (jQueryDummy.e === "#new-document-from") {
                      return "bla";
                  };
                  if (jQueryDummy.e === "#new-document-to") {
                      return "blub";
                  }
              },
              modal : function () {

              }


          };
          spyOn(jQueryDummy, "val").andCallThrough();
          spyOn(jQueryDummy, "modal");
          spyOn(window, "$").andCallFake(function (e) {
              jQueryDummy.e = e;
              return jQueryDummy;

          });

          var arangoDocStoreDummy = {
              createTypeEdge : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeEdge").andReturn("newEdge");
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          window.location.hash = "1/2";
          view.addEdge();
          expect(window.$).toHaveBeenCalledWith("#new-document-from");
          expect(window.$).toHaveBeenCalledWith("#new-document-to");
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
          expect(arangoDocStoreDummy.createTypeEdge).toHaveBeenCalledWith("2", "bla", "blub");
          expect(window.location.hash).toEqual("#collection/newEdge");

      });

      it("addEdge with error", function() {
          jQueryDummy = {
              e : undefined,
              val : function() {
                  if (jQueryDummy.e === "#new-document-from") {
                      return "bla";
                  };
                  if (jQueryDummy.e === "#new-document-to") {
                      return "blub";
                  }
              },
              modal : function () {

              }


          };
          spyOn(jQueryDummy, "val").andCallThrough();
          spyOn(jQueryDummy, "modal");
          spyOn(arangoHelper, "arangoError");
          spyOn(window, "$").andCallFake(function (e) {
              jQueryDummy.e = e;
              return jQueryDummy;

          });

          var arangoDocStoreDummy = {
              createTypeEdge : function() {}
          };
          spyOn(arangoDocStoreDummy, "createTypeEdge").andReturn(false);
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          window.location.hash = "1/2";
          view.addEdge();
          expect(window.$).toHaveBeenCalledWith("#new-document-from");
          expect(window.$).toHaveBeenCalledWith("#new-document-to");
          expect(jQueryDummy.val).toHaveBeenCalled();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Creating edge failed');
          expect(jQueryDummy.modal).not.toHaveBeenCalled();
          expect(arangoDocStoreDummy.createTypeEdge).toHaveBeenCalledWith("2", "bla", "blub");
          expect(window.location.hash).toEqual("#1/2");

      });

      it("first-last-next-prev document", function() {
          var arangoDocumentsStoreDummy = {
              getFirstDocuments : function() {},
              getLastDocuments : function() {},
              getPrevDocuments : function() {},
              getNextDocuments : function() {}
          };
          spyOn(arangoDocumentsStoreDummy, "getFirstDocuments");
          spyOn(arangoDocumentsStoreDummy, "getLastDocuments");
          spyOn(arangoDocumentsStoreDummy, "getPrevDocuments");
          spyOn(arangoDocumentsStoreDummy, "getNextDocuments");
          spyOn(window, "arangoDocuments").andReturn(arangoDocumentsStoreDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();

          view.firstDocuments();
          expect(arangoDocumentsStoreDummy.getFirstDocuments).toHaveBeenCalled();
          view.lastDocuments();
          expect(arangoDocumentsStoreDummy.getLastDocuments).toHaveBeenCalled();
          view.prevDocuments();
          expect(arangoDocumentsStoreDummy.getPrevDocuments).toHaveBeenCalled();
          view.nextDocuments();
          expect(arangoDocumentsStoreDummy.getNextDocuments).toHaveBeenCalled();


      });


      it("remove", function() {
          jQueryDummy = {
              e : undefined,
              prev : function() {
                  return jQueryDummy;
              },
              attr : function () {

              },
              modal : function () {

              }


          };
          spyOn(jQueryDummy, "prev").andCallThrough();
          spyOn(jQueryDummy, "attr");
          spyOn(jQueryDummy, "modal");
          spyOn(window, "$").andReturn(jQueryDummy);

          view.remove({currentTarget : {parentElement : "#thiselement"}});

          expect(window.$).toHaveBeenCalledWith("#thiselement");
          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(window.$).toHaveBeenCalledWith("#docDeleteModal");
          expect(jQueryDummy.prev).toHaveBeenCalled();
          expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled", false);
          expect(jQueryDummy.modal).toHaveBeenCalledWith("show");
          expect(view.alreadyClicked).toEqual(true);
          expect(view.idelement).toEqual(jQueryDummy);

      });


      it("confirmDelete with check != source", function() {
          jQueryDummy = {
              attr : function () {

              }
          };
          spyOn(jQueryDummy, "attr");
          spyOn(window, "$").andReturn(jQueryDummy);
          window.location.hash = "1/2/3/4";
          spyOn(view, "reallyDelete");
          view.confirmDelete();

          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled", true);
          expect(view.reallyDelete).toHaveBeenCalled();
      });


      it("confirmDelete with check = source", function() {
          jQueryDummy = {
              attr : function () {

              }
          };
          spyOn(jQueryDummy, "attr");
          spyOn(window, "$").andReturn(jQueryDummy);
          window.location.hash = "1/2/3/source";
          spyOn(view, "reallyDelete");
          view.confirmDelete();

          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled", true);
          expect(view.reallyDelete).not.toHaveBeenCalled();
      });


      it("reallyDelete a document with error", function() {
          jQueryDummy = {
              closest : function () {
                return jQueryDummy;
              },
              get : function() {

              },
              next : function() {
                return jQueryDummy;
              },
              text : function() {

              }
          };
          spyOn(jQueryDummy, "closest").andCallThrough();
          spyOn(jQueryDummy, "get");
          spyOn(jQueryDummy, "next").andCallThrough();
          spyOn(jQueryDummy, "text").andReturn("4");
          spyOn(window, "$").andReturn(jQueryDummy);
          window.location.hash = "1/2/3/source";

          view.type = "document";
          view.colid = "collection";

          spyOn(arangoHelper, "arangoError");
          var arangoDocStoreDummy = {
              deleteDocument : function() {}
          };
          spyOn(arangoDocStoreDummy, "deleteDocument").andReturn(false);
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          var arangoDocsStoreDummy = {
              getDocuments : function() {}
          };
          spyOn(arangoDocsStoreDummy, "getDocuments");
          spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();
          view.target = "#confirmDeleteBtn";

          view.reallyDelete();

          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(arangoDocsStoreDummy.getDocuments).not.toHaveBeenCalled();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Doc error');
          expect(arangoDocStoreDummy.deleteDocument).toHaveBeenCalledWith("collection", "4");
      });


      it("reallyDelete a edge with error", function() {
          jQueryDummy = {
              closest : function () {
                  return jQueryDummy;
              },
              get : function() {

              },
              next : function() {
                  return jQueryDummy;
              },
              text : function() {

              }
          };
          spyOn(jQueryDummy, "closest").andCallThrough();
          spyOn(jQueryDummy, "get");
          spyOn(jQueryDummy, "next").andCallThrough();
          spyOn(jQueryDummy, "text").andReturn("4");
          spyOn(window, "$").andReturn(jQueryDummy);
          window.location.hash = "1/2/3/source";

          view.type = "edge";
          view.colid = "collection";

          spyOn(arangoHelper, "arangoError");
          var arangoDocStoreDummy = {
              deleteEdge : function() {}
          };
          spyOn(arangoDocStoreDummy, "deleteEdge").andReturn(false);
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          var arangoDocsStoreDummy = {
              getDocuments : function() {}
          };
          spyOn(arangoDocsStoreDummy, "getDocuments");
          spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();
          view.target = "#confirmDeleteBtn";

          view.reallyDelete();

          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Edge error');
          expect(arangoDocsStoreDummy.getDocuments).not.toHaveBeenCalled();
          expect(arangoDocStoreDummy.deleteEdge).toHaveBeenCalledWith("collection", "4");
      });


      it("reallyDelete a document with no error", function() {
          jQueryDummy = {
              closest : function () {
                  return jQueryDummy;
              },
              get : function() {

              },
              next : function() {
                  return jQueryDummy;
              },
              text : function() {

              },
              dataTable : function() {
                  return jQueryDummy;
              },
              fnDeleteRow : function() {

              },
              fnGetPosition : function() {

              },
              fnClearTable  : function() {

              },
              modal  : function() {

              }

          };
          spyOn(jQueryDummy, "closest").andCallThrough();
          spyOn(jQueryDummy, "get").andReturn(1);
          spyOn(jQueryDummy, "fnClearTable");
          spyOn(jQueryDummy, "modal");
          spyOn(jQueryDummy, "fnGetPosition").andReturn(3);
          spyOn(jQueryDummy, "next").andCallThrough();
          spyOn(jQueryDummy, "dataTable").andCallThrough();
          spyOn(jQueryDummy, "fnDeleteRow").andCallThrough();
          spyOn(jQueryDummy, "text").andReturn("4");
          spyOn(window, "$").andReturn(jQueryDummy);
          window.location.hash = "1/2/3/source";

          view.type = "document";
          view.colid = "collection";

          spyOn(arangoHelper, "arangoError");
          var arangoDocStoreDummy = {
              deleteDocument : function() {}
          };
          spyOn(arangoDocStoreDummy, "deleteDocument").andReturn(true);
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          var arangoDocsStoreDummy = {
              getDocuments : function() {}
          };
          spyOn(arangoDocsStoreDummy, "getDocuments");
          spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();
          view.target = "#confirmDeleteBtn";

          view.reallyDelete();

          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
          expect(window.$).toHaveBeenCalledWith("#documentsTableID");
          expect(window.$).toHaveBeenCalledWith("#docDeleteModal");

          expect(jQueryDummy.modal).toHaveBeenCalledWith("hide");
          expect(jQueryDummy.dataTable).toHaveBeenCalled();
          expect(jQueryDummy.fnGetPosition).toHaveBeenCalledWith(1);
          expect(jQueryDummy.fnDeleteRow).toHaveBeenCalledWith(3);
          expect(jQueryDummy.fnClearTable).toHaveBeenCalledWith();
          expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();


          expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
          expect(arangoHelper.arangoError).not.toHaveBeenCalledWith('Doc error');
          expect(arangoDocStoreDummy.deleteDocument).toHaveBeenCalledWith("collection", "4");
      });


      it("reallyDelete a edge with no error", function() {
          jQueryDummy = {
              closest : function () {
                  return jQueryDummy;
              },
              get : function() {

              },
              next : function() {
                  return jQueryDummy;
              },
              text : function() {

              },
              dataTable : function() {
                  return jQueryDummy;
              },
              fnDeleteRow : function() {

              },
              fnGetPosition : function() {

              },
              fnClearTable  : function() {

              },
              modal  : function() {

              }

          };
          spyOn(jQueryDummy, "closest").andCallThrough();
          spyOn(jQueryDummy, "get").andReturn(1);
          spyOn(jQueryDummy, "fnClearTable");
          spyOn(jQueryDummy, "modal");
          spyOn(jQueryDummy, "fnGetPosition").andReturn(3);
          spyOn(jQueryDummy, "next").andCallThrough();
          spyOn(jQueryDummy, "dataTable").andCallThrough();
          spyOn(jQueryDummy, "fnDeleteRow").andCallThrough();
          spyOn(jQueryDummy, "text").andReturn("4");
          spyOn(window, "$").andReturn(jQueryDummy);
          window.location.hash = "1/2/3/source";

          view.type = "edge";
          view.colid = "collection";

          spyOn(arangoHelper, "arangoError");
          var arangoDocStoreDummy = {
              deleteEdge : function() {}
          };
          spyOn(arangoDocStoreDummy, "deleteEdge").andReturn(true);
          spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
          window.arangoDocumentStore = new window.arangoDocument();

          var arangoDocsStoreDummy = {
              getDocuments : function() {}
          };
          spyOn(arangoDocsStoreDummy, "getDocuments");
          spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
          window.arangoDocumentsStore = new window.arangoDocuments();
          view.target = "#confirmDeleteBtn";

          view.reallyDelete();

          expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");


          expect(window.$).toHaveBeenCalledWith("#documentsTableID");
          expect(window.$).toHaveBeenCalledWith("#docDeleteModal");

          expect(jQueryDummy.modal).toHaveBeenCalledWith("hide");
          expect(jQueryDummy.dataTable).toHaveBeenCalled();
          expect(jQueryDummy.fnGetPosition).toHaveBeenCalledWith(1);
          expect(jQueryDummy.fnDeleteRow).toHaveBeenCalledWith(3);
          expect(jQueryDummy.fnClearTable).toHaveBeenCalledWith();
          expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();

          expect(arangoHelper.arangoError).not.toHaveBeenCalledWith('Edge error');
          expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
          expect(arangoDocStoreDummy.deleteEdge).toHaveBeenCalledWith("collection", "4");
      });


      it("clicked when alreadyClicked", function() {
          view.alreadyClicked = true;
          expect(view.clicked("anything")).toEqual(0);
      });

      it("clicked when clicked event has no target", function() {
          jQueryDummy = {
              dataTable : function () {
                  return jQueryDummy;
              },
              fnGetPosition : function () {

              }

          };
          spyOn(jQueryDummy, "dataTable").andCallThrough();
          spyOn(jQueryDummy, "fnGetPosition").andReturn(null);
          spyOn(window, "$").andReturn(jQueryDummy);
          view.alreadyClicked = false;
          expect(view.clicked("anything")).toEqual(undefined);
      });

      it("clicked when clicked event has target but no valid checkData", function() {
          jQueryDummy = {
              dataTable : function () {
                  return jQueryDummy;
              },
              fnGetPosition : function () {

              },
              fnGetData : function () {

              }
          };
          spyOn(jQueryDummy, "dataTable").andCallThrough();
          spyOn(jQueryDummy, "fnGetPosition").andReturn(1);
          spyOn(jQueryDummy, "fnGetData").andReturn([0, ""]);
          spyOn(window, "$").andReturn(jQueryDummy);
          spyOn(view, "addDocument");
          view.alreadyClicked = false;
          expect(view.clicked("anything")).toEqual(undefined);
          expect(view.addDocument).toHaveBeenCalled();
      });

      it("clicked when clicked event has target and valid checkData", function() {
          jQueryDummy = {
              dataTable : function () {
                  return jQueryDummy;
              },
              fnGetPosition : function () {

              },
              fnGetData : function () {

              },
              next : function () {
                  return jQueryDummy;
              },
              text : function () {

              }

          };
          spyOn(jQueryDummy, "dataTable").andCallThrough();
          spyOn(jQueryDummy, "next").andCallThrough();
          spyOn(jQueryDummy, "text").andReturn(12);
          spyOn(jQueryDummy, "fnGetPosition").andReturn(1);
          spyOn(jQueryDummy, "fnGetData").andReturn([0, "true"]);
          spyOn(window, "$").andReturn(jQueryDummy);
          spyOn(view, "addDocument");
          view.alreadyClicked = false;
          view.colid = "coll";
          expect(view.clicked({currentTarget : {firstChild : "blub"}})).toEqual(undefined);
          expect(view.addDocument).not.toHaveBeenCalled();
          expect(window.$).toHaveBeenCalledWith("blub");
          expect(window.location.hash).toEqual("#collection/coll/12");
      });

      it("initTable", function() {
          jQueryDummy = {
              dataTable : function () {
              }
          };
          spyOn(jQueryDummy, "dataTable");
          spyOn(window, "$").andReturn(jQueryDummy);
          view.initTable();
          expect(jQueryDummy.dataTable).toHaveBeenCalledWith({
              "bSortClasses": false,
              "bFilter": false,
              "bPaginate":false,
              "bRetrieve": true,
              "bSortable": false,
              "bSort": false,
              "bLengthChange": false,
              "bAutoWidth": false,
              "iDisplayLength": -1,
              "bJQueryUI": false,
              "aoColumns": [
                  { "sClass":"docsFirstCol","bSortable": false},
                  { "sClass":"docsSecCol", "bSortable": false},
                  { "bSortable": false, "sClass": "docsThirdCol"}
              ],
              "oLanguage": { "sEmptyTable": "Loading..."}
          });
      });

      it("clearTable", function() {
          jQueryDummy = {
              dataTable : function () {
                  return jQueryDummy;
              },
              fnClearTable : function () {
              }
          };
          spyOn(jQueryDummy, "dataTable").andCallThrough();
          spyOn(jQueryDummy, "fnClearTable");
          spyOn(window, "$").andReturn(jQueryDummy);
          view.table = "blub";
          view.clearTable();
          expect(window.$).toHaveBeenCalledWith("blub");
          expect(jQueryDummy.fnClearTable).toHaveBeenCalled();
      });
      /*

              drawTable: function() {
                  var self = this;

                  *//*
                   if (this.addDocumentSwitch === true) {
                   $(self.table).dataTable().fnAddData(
                   [
                   '<a id="plusIconDoc" style="padding-left: 30px">Add document</a>',
                   '',
                   '<img src="img/plus_icon.png" id="documentAddBtn"></img>'
                   ]
                   );
                   }*//*

                  if (window.arangoDocumentsStore.models.length === 0) {
                      $('.dataTables_empty').text('No documents');
                  }
                  else {

                      $.each(window.arangoDocumentsStore.models, function(key, value) {

                          var tempObj = {};
                          $.each(value.attributes.content, function(k, v) {
                              if (! (k === '_id' || k === '_rev' || k === '_key')) {
                                  tempObj[k] = v;
                              }
                          });

                          $(self.table).dataTable().fnAddData(
                              [
                                  '<pre class="prettify" title="'
                                      + self.escaped(JSON.stringify(tempObj))
                                      + '">'
                                      + self.cutByResolution(JSON.stringify(tempObj))
                                      + '</pre>',

                                  '<div class="key">'
                                      + value.attributes.key
                                      + '</div>',

                                  '<a id="deleteDoc" class="deleteButton">'
                                      + '<span class="icon_arangodb_roundminus" data-original-title="'
                                      +'Delete document" title="Delete document"></span><a>'
                              ]
                          );
                      });

                      // we added some icons, so we need to fix their tooltips
                      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");

                      $(".prettify").snippet("javascript", {
                          style: "nedit",
                          menu: false,
                          startText: false,
                          transparent: true,
                          showNum: false
                      });

                  }
                  this.totalPages = window.arangoDocumentsStore.totalPages;
                  this.currentPage = window.arangoDocumentsStore.currentPage;
                  this.documentsCount = window.arangoDocumentsStore.documentsCount;
              },


              render: function() {
                  this.collectionContext = window.arangoCollectionsStore.getPosition(this.colid);

                  $(this.el).html(this.template.render({}));
                  this.getIndex();
                  this.initTable();
                  this.breadcrumb();
                  if (this.collectionContext.prev === null) {
                      $('#collectionPrev').parent().addClass('disabledPag');
                  }
                  if (this.collectionContext.next === null) {
                      $('#collectionNext').parent().addClass('disabledPag');
                  }

                  this.uploadSetup();

                  $("[data-toggle=tooltip]").tooltip();

                  $('.modalImportTooltips').tooltip({
                      placement: "left"
                  });

                  arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");

                  return this;
              },
              showLoadingState: function () {
                  $('.dataTables_empty').text('Loading...');
              },
              renderPagination: function (totalPages, checkFilter) {
                  $('#documentsToolbarF').html("");
                  var self = this;

                  var currentPage;
                  if (checkFilter) {
                      currentPage = window.arangoDocumentsStore.currentFilterPage;
                  }
                  else {
                      currentPage = JSON.parse(this.pageid);
                  }
                  var target = $('#documentsToolbarF'),
                      options = {
                          left: 2,
                          right: 2,
                          page: currentPage,
                          lastPage: totalPages,
                          click: function(i) {
                              options.page = i;
                              if (checkFilter) {
                                  var filterArray = self.getFilterContent();
                                  var filters = filterArray[0];
                                  var bindValues = filterArray[1];
                                  self.addDocumentSwitch = false;

                                  window.documentsView.clearTable();
                                  window.arangoDocumentsStore.getFilteredDocuments(self.colid, i, filters, bindValues);

                                  //Hide first/last pagination
                                  $('#documents_last').css("visibility", "hidden");
                                  $('#documents_first').css("visibility", "hidden");
                              }
                              else {
                                  var windowLocationHash =  '#collection/' + self.colid + '/documents/' + options.page;
                                  window.location.hash = windowLocationHash;
                              }
                          }
                      };
                  target.pagination(options);
                  $('#documentsToolbarF').prepend(
                      '<ul class="pre-pagi"><li><a id="documents_first" class="pagination-button">'+
                          '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>');
                  $('#documentsToolbarF').append(
                      '<ul class="las-pagi"><li><a id="documents_last" class="pagination-button">'+
                          '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>');
                  var total = $('#totalDocuments');
                  if (total.length > 0) {
                      total.html("Total: " + this.documentsCount + " documents");
                  } else {
                      $('#documentsToolbarFL').append(
                          '<a id="totalDocuments" class="totalDocuments">Total: ' + this.documentsCount +
                              ' document(s) </a>'
                      );
                  }
              },
              breadcrumb: function () {
                  this.collectionName = window.location.hash.split("/")[1];
                  $('#transparentHeader').append(
                      '<div class="breadcrumb">'+
                          '<a class="activeBread" href="#collections">Collections</a>'+
                          '  &gt;  '+
                          '<a class="disabledBread">'+this.collectionName+'</a>'+
                          '</div>'
                  );
              },
              cutByResolution: function (string) {
                  if (string.length > 1024) {
                      return this.escaped(string.substr(0, 1024)) + '...';
                  }
                  return this.escaped(string);
              },
              escaped: function (value) {
                  return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
              },
              resetIndexForms: function () {
                  $('#indexHeader input').val('').prop("checked", false);
                  $('#newIndexType').val('Cap').prop('selected',true);
                  this.selectIndexType();
              },
              stringToArray: function (fieldString) {
                  var fields = [];
                  fieldString.split(',').forEach(function(field){
                      field = field.replace(/(^\s+|\s+$)/g,'');
                      if (field !== '') {
                          fields.push(field);
                      }
                  });
                  return fields;
              },
              createIndex: function (e) {
                  //e.preventDefault();
                  var self = this;
                  var collection = this.collectionName;
                  var indexType = $('#newIndexType').val();
                  var result;
                  var postParameter = {};
                  var fields;
                  var unique;

                  switch(indexType) {
                      case 'Cap':
                          var size = parseInt($('#newCapSize').val(), 10) || 0;
                          var byteSize = parseInt($('#newCapByteSize').val(), 10) || 0;
                          postParameter = {
                              type: 'cap',
                              size: size,
                              byteSize: byteSize
                          };
                          break;
                      case 'Geo':
                          //HANDLE ARRAY building
                          fields = $('#newGeoFields').val();
                          var geoJson = self.checkboxToValue('#newGeoJson');
                          var constraint = self.checkboxToValue('#newGeoConstraint');
                          var ignoreNull = self.checkboxToValue('#newGeoIgnoreNull');
                          postParameter = {
                              type: 'geo',
                              fields: self.stringToArray(fields),
                              geoJson: geoJson,
                              constraint: constraint,
                              ignoreNull: ignoreNull
                          };
                          break;
                      case 'Hash':
                          fields = $('#newHashFields').val();
                          unique = self.checkboxToValue('#newHashUnique');
                          postParameter = {
                              type: 'hash',
                              fields: self.stringToArray(fields),
                              unique: unique
                          };
                          break;
                      case 'Fulltext':
                          fields = ($('#newFulltextFields').val());
                          var minLength =  parseInt($('#newFulltextMinLength').val(), 10) || 0;
                          postParameter = {
                              type: 'fulltext',
                              fields: self.stringToArray(fields),
                              minLength: minLength
                          };
                          break;
                      case 'Skiplist':
                          fields = $('#newSkiplistFields').val();
                          unique = self.checkboxToValue('#newSkiplistUnique');
                          postParameter = {
                              type: 'skiplist',
                              fields: self.stringToArray(fields),
                              unique: unique
                          };
                          break;
                  }
                  result = window.arangoCollectionsStore.createIndex(collection, postParameter);
                  if (result === true) {
                      $('#collectionEditIndexTable tr').remove();
                      self.getIndex();
                      self.toggleNewIndexView();
                      self.resetIndexForms();
                  }
                  else {
                      if (result.responseText) {
                          var message = JSON.parse(result.responseText);
                          arangoHelper.arangoNotification("Document error", message.errorMessage);
                      }
                      else {
                          arangoHelper.arangoNotification("Document error", "Could not create index.");
                      }
                  }
              },

              prepDeleteIndex: function (e) {
                  this.lastTarget = e;
                  this.lastId = $(this.lastTarget.currentTarget).
                      parent().
                      parent().
                      first().
                      children().
                      first().
                      text();
                  $("#indexDeleteModal").modal('show');
              },
              deleteIndex: function () {
                  var result = window.arangoCollectionsStore.deleteIndex(this.collectionName, this.lastId);
                  if (result === true) {
                      $(this.lastTarget.currentTarget).parent().parent().remove();
                  }
                  else {
                      arangoHelper.arangoError("Could not delete index");
                  }
                  $("#indexDeleteModal").modal('hide');
              },
              selectIndexType: function () {
                  $('.newIndexClass').hide();
                  var type = $('#newIndexType').val();
                  $('#newIndexType'+type).show();
              },
              checkboxToValue: function (id) {
                  return $(id).prop('checked');
              },
              getIndex: function () {
                  this.index = window.arangoCollectionsStore.getIndex(this.collectionID, true);
                  var cssClass = 'collectionInfoTh modal-text';
                  if (this.index) {
                      var fieldString = '';
                      var actionString = '';

                      $.each(this.index.indexes, function(k,v) {
                          if (v.type === 'primary' || v.type === 'edge') {
                              actionString = '<span class="icon_arangodb_locked" ' +
                                  'data-original-title="No action"></span>';
                          }
                          else {
                              actionString = '<span class="deleteIndex icon_arangodb_roundminus" ' +
                                  'data-original-title="Delete index" title="Delete index"></span>';
                          }

                          if (v.fields !== undefined) {
                              fieldString = v.fields.join(", ");
                          }

                          //cut index id
                          var position = v.id.indexOf('/');
                          var indexId = v.id.substr(position+1, v.id.length);

                          $('#collectionEditIndexTable').append(
                              '<tr>'+
                                  '<th class=' + JSON.stringify(cssClass) + '>' + indexId + '</th>'+
                                  '<th class=' + JSON.stringify(cssClass) + '>' + v.type + '</th>'+
                                  '<th class=' + JSON.stringify(cssClass) + '>' + v.unique + '</th>'+
                                  '<th class=' + JSON.stringify(cssClass) + '>' + fieldString + '</th>'+
                                  '<th class=' + JSON.stringify(cssClass) + '>' + actionString + '</th>'+
                                  '</tr>'
                          );
                      });

                      arangoHelper.fixTooltips("deleteIndex", "left");
                  }
              }
          });
      }());*/
  });

}());
