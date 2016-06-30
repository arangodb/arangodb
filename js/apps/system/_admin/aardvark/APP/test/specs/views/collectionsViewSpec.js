/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global runs, waitsFor, jasmine, waits*/
/* global $, console, arangoHelper */
(function () {
  'use strict';
  describe('Collections View', function () {
    var myStore,
      isCoordinator,
      myView,
      div,
      edgeCol,
      docCol,
      sysCol;

    beforeEach(function () {
      isCoordinator = false;
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
      edgeCol = new window.arangoCollectionModel({
        id: 'e',
        type: 'edge',
        isSystem: false,
        name: 'e',
        status: 'loaded'
      });
      docCol = new window.arangoCollectionModel({
        id: 'd',
        type: 'document',
        isSystem: false,
        name: 'd',
        status: 'loaded'
      });
      sysCol = new window.arangoCollectionModel({
        id: 's',
        type: 'document',
        isSystem: true,
        name: '_sys',
        status: 'loaded'
      });

      var cols = [edgeCol, docCol, sysCol];
      spyOn($, 'ajax').andCallFake(function (url) {
        return {done: function () {}};
      });
      myStore = new window.arangoCollections(cols);
      spyOn(window, 'isCoordinator').andReturn(isCoordinator);
      myView = new window.CollectionsView({
        collection: myStore
      });
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('Check Collection Display Settings', function () {
      it('Check if System Collections should not be drawn', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.checkSystem();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if System Collections should be drawn', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });
        $('.checkSystemCollections').click();
        myView.checkSystem();
        expect(wasRendered).toBeTruthy();
      });

      it('Check if Edge Collections should not be drawn', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.checkEdge();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Edge Collections should be drawn', function () {
        myView.render();
        var wasRendered;
        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });

        $('#checkEdge').click();
        myView.checkEdge();
        expect(wasRendered).toBeTruthy();
      });

      it('Check if Document Collections should not be drawn', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.checkDocument();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Document Collections should be drawn', function () {
        myView.render();
        var wasRendered;
        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });

        $('#checkDocument').click();
        myView.checkDocument();
        expect(wasRendered).toBeTruthy();
      });

      it('Check if Loaded Collections should not be drawn', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.checkLoaded();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Loaded Collections should be drawn', function () {
        myView.render();
        var wasRendered;
        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });

        $('#checkLoaded').click();
        myView.checkLoaded();
        expect(wasRendered).toBeTruthy();
      });

      it('Check if Unloaded Collections should not be drawn', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.checkUnloaded();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Unloaded Collections should be drawn', function () {
        myView.render();
        var wasRendered;
        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });

        $('#checkUnloaded').click();
        expect(wasRendered).toBeTruthy();
        wasRendered = false;
        myView.checkUnloaded();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Name Sort is inactive', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.sortName();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Name Sort is active', function () {
        var old = myView.collection.searchOptions,
          wasRendered;
        myView.render();
        myView.collection.searchOptions = {sortBy: [1, 2, 3]};

        spyOn(myView, 'render').andCallFake(function () {
          wasRendered = true;
        });
        myView.sortName();
        expect(wasRendered).toBeTruthy();
        myView.collection.searchOptions = old;
      });

      it('Check if Type is inactive', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.sortType();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if Type is active', function () {
        var old = myView.collection.searchOptions,
          wasRendered;
        myView.render();
        myView.collection.searchOptions = {sortBy: [1, 2, 3]};

        spyOn(myView, 'render').andCallFake(function () {
          wasRendered = true;
        });
        myView.sortType();
        expect(wasRendered).toBeTruthy();
        myView.collection.searchOptions = old;
      });

      it('Check if sortOrder is inactive', function () {
        myView.render();
        var wasRendered;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = false;
        });
        myView.sortOrder();
        expect(wasRendered).toBeFalsy();
      });

      it('Check if sortOrder is active', function () {
        var old = myView.collection.searchOptions,
          wasRendered;
        myView.render();
        myView.collection.searchOptions = {sortOrder: [1, 2, 3]};

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });
        myView.sortOrder();
        expect(wasRendered).toBeTruthy();
        myView.collection.searchOptions = old;
      });

      it('Check Filter Values', function () {
        myView.render();
        var check = false;
        $.each(myView.collection.searchOptions, function () {
          if (this === undefined) {
            check = false;
            return;
          }
          check = true;
        });
        expect(check).toBeTruthy();
      });
    });

    describe('Check Collection Search Functions', function () {
      it('Check if search needs to be executed (false)', function () {
        myView.render();
        var wasRendered = false,
          old;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });
        old = myView.collection.searchOptions;
        $('#searchInput').val('HalloPeter');
        myView.collection.searchOptions.searchPhrase = 'HalloPeter';
        myView.search();
        expect(wasRendered).toBeFalsy();
        myView.collection.searchOptions = old;
      });

      it('Check if search needs to be executed (true)', function () {
        myView.render();
        var wasRendered = false;

        spyOn(myView, 'render').andCallFake(function (request) {
          wasRendered = true;
        });

        $('#searchInput').val('HalloPeter');
        myView.collection.searchOptions.searchPhrase = 'HalloMax';
        myView.search();

        expect(wasRendered).toBeTruthy();
      });

      it('Reset search phrase', function () {
        myView.render();
        myView.resetSearch();
        expect(myView.collection.searchOptions.searchPhrase).toBe(null);
      });

      it('Reset search timeout', function () {
        myView.render();
        myView.searchTimeout = 1;
        myView.resetSearch();
        expect(myView.searchTimeout).toBe(null);
      });

      it('Check restrictToSearchPhraseKey function', function () {
        myView.render();
        var resetSearch, newSearch;

        spyOn(myView, 'resetSearch').andCallFake(function (request) {
          resetSearch = true;
        });

        spyOn(myView, 'search').andCallFake(function (request) {
          newSearch = true;
        });

        runs(function () {
          myView.restrictToSearchPhraseKey();
        });
        waits(300);
        runs(function () {
          expect(resetSearch).toBeTruthy();
          expect(newSearch).toBeTruthy();
        });
      });

      it('test toggleView', function () {
        myView.render();
        var a = {
          toggleClass: function () {},
          slideToggle: function () {}
        };

        spyOn(window, '$').andReturn(a);

        spyOn(a, 'toggleClass');
        spyOn(a, 'slideToggle');
        myView.toggleView();
        expect(a.toggleClass).toHaveBeenCalledWith('activated');
        expect(a.slideToggle).toHaveBeenCalledWith(200);
      });

      it('test checkBoxes', function () {
        myView.render();
        var e = {
            currentTarget: {
              id: 1
            }
          },
          a = {
            click: function () {}
        };

        spyOn(window, '$').andReturn(a);

        spyOn(a, 'click');
        myView.checkBoxes(e);
        expect(a.click).toHaveBeenCalled();
      });

    /* TODO Fix this
    it(
        "test set #collectionsToggle is deactivated when " +
            "#collectionsDropdown2 is invisible",
        function () {
            var a = {
                css: function () {
                    return 'none'
                },
                is: function () {
                    return true
                },
                show: function () {
                },
                append: function () {
                    return {
                        render: function () {
                            return {el: 1}
                        }
                    }
                },
                removeClass: function () {
                },
                val: function () {
                },
                focus: function () {
                },
                html: function () {
                },
                attr: function () {
                }

            }
            spyOn(window, "CollectionListItemView").andReturn({
                render: function () {
                    return {el: 1}
                }
            })
            spyOn(arangoHelper, "fixTooltips").andReturn()
            spyOn(window, "$").andReturn(a)
            spyOn(a, "removeClass")

            myView.render()
            expect(a.removeClass).toHaveBeenCalledWith('activated')
        })
      */
    });

    it('test restrictToSearchPhrase', function () {
      myView.render();
      spyOn(myView, 'resetSearch');
      spyOn(myView, 'search');

      myView.restrictToSearchPhrase();
      expect(myView.resetSearch).toHaveBeenCalled();
      expect(myView.search).toHaveBeenCalled();
    });
  });
}());
