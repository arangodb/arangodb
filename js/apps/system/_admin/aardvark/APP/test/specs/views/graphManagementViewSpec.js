/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, runs, expect, waitsFor, arangoHelper*/
/* global GraphManagementView, _, jasmine, $*/

(function () {
  'use strict';

  describe('Graph Management View', function () {
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
      modalDiv = document.createElement('div');
      modalDiv.id = 'modalPlaceholder';
      document.body.appendChild(modalDiv);
      window.modalView = new window.ModalView();
      collections = new window.arangoCollections(cols);
      graphs = new window.GraphCollection();
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
      view = new window.GraphManagementView({
        collection: graphs,
        collectionCollection: new window.arangoCollections()
      });
    });

    afterEach(function () {
      delete window.modalView;
      document.body.removeChild(div);
      document.body.removeChild(modalDiv);
    });

    it('should updateGraphManagementView', function () {
      spyOn(view.collection, 'fetch').andCallFake(function (a) {
        a.success();
      });

      spyOn(view, 'render');

      view.updateGraphManagementView();

      expect(view.render).toHaveBeenCalled();
    });

    it('should fetch the graphs on render', function () {
      spyOn(graphs, 'fetch');
      view.render();
      expect(graphs.fetch).toHaveBeenCalledWith({async: false});
    });

    describe('after rendering', function () {
      var g1, g2, g3, g4;

      beforeEach(function () {
        g1 = {
          _id: '_graphs/g1',
          _key: 'g1',
          _rev: '123',
          edgeDefinitions: [
            {
              collection: e1,
              from: ['f1'],
              to: ['t1']

            }
          ],
          orphanCollections: ['o1']
        };
        g2 = {
          _id: '_graphs/g2',
          _key: 'g2',
          _rev: '321',
          edgeDefinitions: [
            {
              collection: e2,
              from: ['f2'],
              to: ['t2']

            }
          ],
          orphanCollections: ['o2']
        };
        g3 = {
          _id: '_graphs/g3',
          _key: 'g3',
          _rev: '111',
          edgeDefinitions: [
            {
              collection: e3,
              from: ['f3'],
              to: ['t3']

            }
          ],
          orphanCollections: ['o3']
        };
        g4 = {
          _id: '_graphs/g4',
          _key: 'g4',
          _rev: '111',
          edgeDefinitions: [],
          orphanCollections: []
        };
        spyOn(graphs, 'fetch');
        graphs.add(g1);
        graphs.add(g2);
        graphs.add(g3);
        graphs.add(g4);
        view.render();
      });

      it('should load the GraphViewer', function () {
        var e = {
            currentTarget: {
              id: 'g1_tile'
            }
          },
          a = {
            attr: function () {
              return 'g1_tile';
            },
            width: function () {
              return 100;
            },
            html: function () {}
        };
        spyOn(window, 'GraphViewerUI');

        spyOn(window, '$').andReturn(a);

        view.loadGraphViewer(e);

        expect(window.GraphViewerUI).toHaveBeenCalledWith(
          undefined,
          {
            type: 'gharial',
            graphName: 'g1',
            baseUrl: '/_db/_system/'
          }, 25, 680,
          {
            nodeShaper: {
              label: '_key',
              color: { type: 'attribute', key: '_key' }
            }
          }, true
        );
      });

      it('should not load the GraphViewer for empty graphs', function () {
        var e = {
            currentTarget: {
              id: 'g4_tile'
            }
          },
          a = {
            attr: function () {
              return 'g4_tile';
            },
            width: function () {
              return 100;
            },
            html: function () {}
        };
        spyOn(window, 'GraphViewerUI');

        spyOn(window, '$').andReturn(a);

        view.loadGraphViewer(e);

        expect(window.GraphViewerUI).not.toHaveBeenCalled();
      });

      it('should showHideDefinition', function () {
        var e = {
            currentTarget: {
              id: 'row_newEdgeDefinitions1'
            },
            stopPropagation: function () {}
          },
          a = {
            attr: function () {
              return 'row_newEdgeDefinitions1';
            },
            toggle: function () {}
        };
        spyOn(a, 'toggle');

        spyOn(window, '$').andReturn(a);

        view.showHideDefinition(e);

        expect(a.toggle).toHaveBeenCalled();
      });

      it('should addDefinition', function () {
        var e = {
            currentTarget: {
              id: 'row_newEdgeDefinitions1'
            },
            stopPropagation: function () {}
          },
          a = {
            attr: function (x) {
              return 'addAfter_newEdgeDefinitions1';
            },
            toggle: function () {},
            before: function () {},
            select2: function () {}
        };

        spyOn(view.edgeDefintionTemplate, 'render');

        spyOn(window, '$').andReturn(a);

        spyOn(window.modalView, 'undelegateEvents');
        spyOn(window.modalView, 'delegateEvents');

        view.options.collectionCollection.add(
          {name: 'NONSYSTEM', isSystem: false, type: 'edge'});
        view.options.collectionCollection.add(
          {name: 'SYSTEM', isSystem: true, type: 'document'});

        view.counter = 0;

        view.addRemoveDefinition(e);

        expect(window.modalView.undelegateEvents).toHaveBeenCalled();
        expect(window.modalView.delegateEvents).toHaveBeenCalled();
        expect(view.edgeDefintionTemplate.render).toHaveBeenCalledWith({number: 1});
      });

      it('should removeDefinition', function () {
        var e = {
            currentTarget: {
              id: 'row_newEdgeDefinitions1'
            },
            stopPropagation: function () {}
          },
          a = {
            attr: function (x) {
              return 'remove_newEdgeDefinitions1';
            },
            remove: function () {},
            before: function () {},
            select2: function () {}
        };

        spyOn(view.edgeDefintionTemplate, 'render');

        spyOn(window, '$').andReturn(a);

        spyOn(window.modalView, 'undelegateEvents');
        spyOn(window.modalView, 'delegateEvents');

        view.options.collectionCollection.add(
          {name: 'NONSYSTEM', isSystem: false, type: 'edge'});
        view.options.collectionCollection.add(
          {name: 'SYSTEM', isSystem: true, type: 'document'});

        view.counter = 0;

        view.addRemoveDefinition(e);

        expect(window.modalView.undelegateEvents).not.toHaveBeenCalled();
        expect(window.modalView.delegateEvents).not.toHaveBeenCalled();
        expect(view.edgeDefintionTemplate.render).not.toHaveBeenCalled();
      });

      it('should handle resize', function () {
        var ui = {
          changeWidth: function () {}
        };
        spyOn(ui, 'changeWidth');
        view.ui = ui;

        view.handleResize(1);
        expect(view.ui.changeWidth).toHaveBeenCalledWith(1);
      });

      it('should delete graph', function () {
        var e = {
            currentTarget: {
              id: 'blabalblub'
            }
          },
          a = [{
            value: 'blabalblub'
          }],collReturn = {
            destroy: function (a) {
              a.success();
            }
        };

        spyOn(graphs, 'get').andReturn(collReturn);

        spyOn(window, '$').andReturn(a);

        spyOn(window.modalView, 'hide').andReturn(a);

        view.deleteGraph(e);

        expect(graphs.get).toHaveBeenCalledWith('blabalblub');
        expect(window.modalView.hide).toHaveBeenCalled();
      });

      it('should NOT delete graph', function () {
        var e = {
            currentTarget: {
              id: 'blabalblub'
            }
          },
          a = [{
            value: 'blabalblub'
          }],collReturn = {
            destroy: function (a) {
              a.error('', {responseText: '{"errorMessage" : "errorMessage"}'});
            }
        };

        spyOn(graphs, 'get').andReturn(collReturn);

        spyOn(window, '$').andReturn(a);
        spyOn(arangoHelper, 'arangoError');

        spyOn(window.modalView, 'hide').andReturn(a);

        view.deleteGraph(e);

        expect(graphs.get).toHaveBeenCalledWith('blabalblub');
        expect(arangoHelper.arangoError).toHaveBeenCalledWith('errorMessage');
        expect(window.modalView.hide).toHaveBeenCalled();
      });

      it('should set to and from for added definition', function () {
        view.removedECollList = [];
        var model = view.collection.create({
            _key: 'blub',
            name: 'blub',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: []
          }),
          e = {
            currentTarget: {
              id: 'blabalblub'
            },
            stopPropagation: function () {},
            added: {
              id: 'moppel'

            },
            val: 'newEdgeDefintion'
          },
          a = {
            select2: function () {},
            attr: function () {},
            length: 2,
            0: {id: 1},
            1: {id: 2}
          },collReturn = {
            destroy: function (a) {
              a.error('', {responseText: '{"errorMessage" : "errorMessage"}'});
            }
        };

        spyOn(e, 'stopPropagation');

        spyOn(window, '$').andReturn(a);

        spyOn(window.modalView, 'hide').andReturn(a);

        view.setFromAndTo(e);

        expect(e.stopPropagation).toHaveBeenCalled();

        model.destroy();
      });

      it('should set to and from for added definition for already known def', function () {
        view.removedECollList = [];
        var model = view.collection.create({
            _key: 'blub2',
            name: 'blub2',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: []
          }), e = {
            currentTarget: {
              id: 'blabalblub'
            },
            stopPropagation: function () {},
            added: {
              id: 'moppel'

            },
            val: 'blub'
          },
          a = {
            select2: function () {},
            attr: function () {},
            length: 2,
            0: {id: 1},
            1: {id: 2}
          },collReturn = {
            destroy: function (a) {
              a.error('', {responseText: '{"errorMessage" : "errorMessage"}'});
            }
        };

        spyOn(e, 'stopPropagation');

        spyOn(window, '$').andReturn(a);

        spyOn(window.modalView, 'hide').andReturn(a);

        view.setFromAndTo(e);

        expect(e.stopPropagation).toHaveBeenCalled();

        model.destroy();
      });

      it('should not set to and from for added definition as already in ' +
        'use and has been entered manually', function () {
          view.removedECollList = ['moppel'];
          var model = view.collection.create({
              _key: 'blub2',
              name: 'blub2',
              edgeDefinitions: [{
                collection: 'blub',
                from: ['bla'],
                to: ['blob']
              }],
              orphanCollections: []
            }), e = {
              currentTarget: {
                id: 'blabalblub'
              },
              stopPropagation: function () {},
              added: {
                id: 'moppel'

              },
              val: 'blub'
            },
            a = {
              select2: function () {},
              attr: function () {},
              length: 2,
              0: {id: 1},
              1: {id: 2}
            },collReturn = {
              destroy: function (a) {
                a.error('', {responseText: '{"errorMessage" : "errorMessage"}'});
              }
          };

          spyOn(e, 'stopPropagation');

          spyOn(window, '$').andReturn(a);

          spyOn(window.modalView, 'hide').andReturn(a);

          view.setFromAndTo(e);

          expect(e.stopPropagation).toHaveBeenCalled();

          model.destroy();
        });

      it('should not set to and from for removed definition as ' +
        'already in use and has been entered manually', function () {
          view.removedECollList = ['moppel'];
          var model = view.collection.create({
              _key: 'blub2',
              name: 'blub2',
              edgeDefinitions: [{
                collection: 'blub',
                from: ['bla'],
                to: ['blob']
              }],
              orphanCollections: []
            }), e = {
              currentTarget: {
                id: 'blabalblub'
              },
              stopPropagation: function () {},
              val: 'blub',
              removed: {id: 'moppel'}
            },
            a = {
              select2: function () {},
              attr: function () {},
              length: 2,
              0: {id: 1},
              1: {id: 2}
            },collReturn = {
              destroy: function (a) {
                a.error('', {responseText: '{"errorMessage" : "errorMessage"}'});
              }
          };

          spyOn(e, 'stopPropagation');

          spyOn(window, '$').andReturn(a);

          spyOn(window.modalView, 'hide').andReturn(a);

          view.setFromAndTo(e);

          expect(e.stopPropagation).toHaveBeenCalled();
          expect(view.removedECollList.indexOf('moppel')).toEqual(-1);
          expect(view.eCollList.indexOf('moppel')).not.toEqual(-1);
          model.destroy();
        });

      it('should create edit graph modal in create mode', function () {
        var e = {
          currentTarget: {
            id: 'blabalblub'
          },
          stopPropagation: function () {},
          val: 'blub',
          removed: {id: 'moppel'}
        };

        spyOn(e, 'stopPropagation');
        spyOn(window.modalView, 'show');

        view.options.collectionCollection.add(
          {name: 'NONSYSTEM', isSystem: false, type: 'edge'});
        view.options.collectionCollection.add(
          {name: 'SYSTEM', isSystem: true, type: 'document'});

        view.editGraph(e);

        expect(e.stopPropagation).toHaveBeenCalled();
        expect(view.removedECollList.length).toEqual(0);
        expect(view.eCollList.length).not.toEqual(0);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it('should search', function () {
        var a = {
            val: function () {
              return 'searchString';
            },
            html: function () {
              return '';
            },
            focus: function () {
              return '';
            },
            0: {setSelectionRange: function () {
                return '';
            }}
          }, g = {
            _key: 'blub2',
            name: 'blub2',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'blub2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: [],
            get: function (a) {
              return g[a];
            }
        };

        spyOn(view.collection, 'filter').andCallFake(function (a) {
          a(g);
          return g;
        });

        spyOn(window, '$').andReturn(a);
        spyOn(view.template, 'render');

        view.search();

        expect(view.template.render).toHaveBeenCalledWith({
          graphs: g,
          searchString: 'searchString'
        });
      });

      it('should create edit graph modal in edit mode', function () {
        var g = {
            _key: 'blub2',
            name: 'blub2',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'blub2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: [],
            get: function (a) {
              return g[a];
            }
          }, e = {
            currentTarget: {
              id: 'blabalblub'
            },
            stopPropagation: function () {},
            val: 'blub',
            removed: {id: 'moppel'}
        };

        spyOn(view.collection, 'findWhere').andReturn(
          g
        );

        spyOn(e, 'stopPropagation');
        spyOn(window.modalView, 'show');

        view.options.collectionCollection.add(
          {name: 'NONSYSTEM', isSystem: false, type: 'edge'});
        view.options.collectionCollection.add(
          {name: 'SYSTEM', isSystem: true, type: 'document'});

        view.editGraph(e);

        expect(e.stopPropagation).toHaveBeenCalled();
        expect(view.removedECollList.length).toEqual(1);
        expect(view.eCollList.length).toEqual(0);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it('should create edit graph modal in edit mode without defintions', function () {
        var g = {
            _key: 'blub2',
            name: 'blub2',
            edgeDefinitions: [],
            orphanCollections: [],
            get: function (a) {
              return g[a];
            }
          }, e = {
            currentTarget: {
              id: 'blabalblub'
            },
            stopPropagation: function () {},
            val: 'blub',
            removed: {id: 'moppel'}
        };

        spyOn(view.collection, 'findWhere').andReturn(
          g
        );

        spyOn(e, 'stopPropagation');
        spyOn(window.modalView, 'show');

        view.options.collectionCollection.add(
          {name: 'NONSYSTEM', isSystem: false, type: 'edge'});
        view.options.collectionCollection.add(
          {name: 'SYSTEM', isSystem: true, type: 'document'});

        view.editGraph(e);

        expect(e.stopPropagation).toHaveBeenCalled();
        expect(view.removedECollList.length).toEqual(0);
        expect(view.eCollList.length).toEqual(1);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it('should not saveEditedGraph as no defintions are supplied', function () {
        var a = {
          p: '',
          setParam: function (p) {
            a.p = p;
          },
          select2: function () {
            if (a.p === '#newVertexCollections') {
              return 'newVertexCollections';
            }
            if (a.p === '#s2id_newEdgeDefinitions1') {
              return 'collection';
            }
            if (a.p === '#s2id_fromCollections1') {
              return 'fromCollection';
            }
            if (a.p === '#s2id_toCollections1') {
              return 'toCollection';
            }
          },
          attr: function () {
            if (a.p.indexOf('edgeD') !== -1) {
              return '1';
            }
          },
          toArray: function () {
            if (a.p === '[id^=s2id_newEdgeDefinitions]') {
              return ['edgeD1', 'edgeD2', 'edgeD3'];
            }
          },
          css: function () {},
          0: {value: 'name'}
        };

        spyOn(_, 'pluck').andCallFake(function (s, b) {
          if (s === 'newVertexCollections') {
            return ['orphan1', 'orphan2', 'orphan3'];
          }
          if (s === 'collection') {
            return [undefined];
          }
          if (s === 'fromCollection') {
            return ['fromCollection'];
          }
          if (s === 'toCollection') {
            return ['toCollection'];
          }
        });

        spyOn(window, '$').andCallFake(function (x) {
          a.setParam(x);
          return a;
        });

        spyOn(view, 'updateGraphManagementView');

        view.saveEditedGraph();

        expect(view.updateGraphManagementView).not.toHaveBeenCalled();
      });

      it('should saveEditedGraph', function () {
        var a = {
            p: '',
            setParam: function (p) {
              a.p = p;
            },
            select2: function () {
              if (a.p === '#newVertexCollections') {
                return 'newVertexCollections';
              }
              if (a.p.indexOf('#s2id_newEdgeDefinitions') !== -1) {
                return a.p.split('#s2id_newEdgeDefinitions')[1];
              }
              if (a.p.indexOf('#s2id_fromCollections') !== -1) {
                return 'fromCollection';
              }
              if (a.p.indexOf('#s2id_toCollections') !== -1) {
                return 'toCollection';
              }
            },
            attr: function () {
              if (a.p.indexOf('edgeD') !== -1) {
                return a.p;
              }
            },
            toArray: function () {
              if (a.p === '[id^=s2id_newEdgeDefinitions]') {
                return ['edgeD1', 'edgeD2', 'edgeD3'];
              }
            },
            0: {value: 'name'}
          }, g = {
            _key: 'blub2',
            name: 'blub2',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'collection2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: ['o1', 'o2', 'o3'],
            get: function (a) {
              return g[a];
            },
            deleteVertexCollection: function (o) {
              g.orphanCollections.splice(g.orphanCollections.indexOf(o), 1);
            },
            addVertexCollection: function (o) {
              g.orphanCollections.push(o);
            },
            addEdgeDefinition: function (o) {
              g.edgeDefinitions.push(o);
            },
            modifyEdgeDefinition: function (o) {},
            deleteEdgeDefinition: function (o) {
              g.edgeDefinitions.forEach(function (e) {
                if (e.collection === o.collection) {
                  delete g.edgeDefinitions[e];
                }
              });
            }
        };

        spyOn(_, 'pluck').andCallFake(function (s, b) {
          if (s === 'newVertexCollections') {
            return ['orphan1', 'orphan2', 'orphan3'];
          }
          if (s.indexOf('edgeD') !== -1) {
            return ['collection' + s.split('edgeD')[1]];
          }
          if (s === 'fromCollection') {
            return ['fromCollection'];
          }
          if (s === 'toCollection') {
            return ['toCollection'];
          }
        });

        spyOn(window, '$').andCallFake(function (x) {
          a.setParam(x);
          return a;
        });
        spyOn(view.collection, 'findWhere').andReturn(
          g
        );

        spyOn(view, 'updateGraphManagementView');
        spyOn(window.modalView, 'hide');

        view.saveEditedGraph();

        expect(view.updateGraphManagementView).toHaveBeenCalled();
        expect(window.modalView.hide).toHaveBeenCalled();
      });

      it('should create a sorted list of all graphs', function () {
        var list = $('div.tile h5.collectionName', '#graphManagementThumbnailsIn');
        expect(list.length).toEqual(4);
        // Order would be g2, g3, g1, g4
        expect($(list[0]).html()).toEqual(g1._key);
        expect($(list[1]).html()).toEqual(g2._key);
        expect($(list[2]).html()).toEqual(g3._key);
        expect($(list[3]).html()).toEqual(g4._key);
      });

      it('should not saveCreatedGraph as name already exists', function () {
        var a = {
            p: '',
            setParam: function (p) {
              a.p = p;
            },
            select2: function () {
              if (a.p === '#newVertexCollections') {
                return 'newVertexCollections';
              }
              if (a.p.indexOf('#s2id_newEdgeDefinitions') !== -1) {
                return a.p.split('#s2id_newEdgeDefinitions')[1];
              }
              if (a.p.indexOf('#s2id_fromCollections') !== -1) {
                return 'fromCollection';
              }
              if (a.p.indexOf('#s2id_toCollections') !== -1) {
                return 'toCollection';
              }
            },
            attr: function () {
              if (a.p.indexOf('edgeD') !== -1) {
                return a.p;
              }
            },
            toArray: function () {
              if (a.p === '[id^=s2id_newEdgeDefinitions]') {
                return ['edgeD1', 'edgeD2', 'edgeD3'];
              }
            },
            val: function () {
              return 'name';
            }
          }, g = {
            _key: 'name',
            name: 'name',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'collection2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: ['o1', 'o2', 'o3'],
            get: function (a) {
              return g[a];
            },
            deleteVertexCollection: function (o) {
              g.orphanCollections.splice(g.orphanCollections.indexOf(o), 1);
            },
            addVertexCollection: function (o) {
              g.orphanCollections.push(o);
            },
            addEdgeDefinition: function (o) {
              g.edgeDefinitions.push(o);
            },
            modifyEdgeDefinition: function (o) {},
            deleteEdgeDefinition: function (o) {
              g.edgeDefinitions.forEach(function (e) {
                if (e.collection === o.collection) {
                  delete g.edgeDefinitions[e];
                }
              });
            }
        };

        spyOn(_, 'pluck').andCallFake(function (s, b) {
          if (s === 'newVertexCollections') {
            return ['orphan1', 'orphan2', 'orphan3'];
          }
          if (s.indexOf('edgeD') !== -1) {
            return ['collection' + s.split('edgeD')[1]];
          }
          if (s === 'fromCollection') {
            return ['fromCollection'];
          }
          if (s === 'toCollection') {
            return ['toCollection'];
          }
        });

        spyOn(window, '$').andCallFake(function (x) {
          a.setParam(x);
          return a;
        });

        spyOn(view.collection, 'findWhere').andReturn(
          g
        );

        spyOn(arangoHelper, 'arangoError');

        view.createNewGraph();

        expect(arangoHelper.arangoError).toHaveBeenCalledWith(
          "The graph '" + 'name' + "' already exists.");
      });

      it('should  saveCreatedGraph', function () {
        var a = {
            p: '',
            setParam: function (p) {
              a.p = p;
            },
            select2: function () {
              if (a.p === '#newVertexCollections') {
                return 'newVertexCollections';
              }
              if (a.p.indexOf('#s2id_newEdgeDefinitions') !== -1) {
                return a.p.split('#s2id_newEdgeDefinitions')[1];
              }
              if (a.p.indexOf('#s2id_fromCollections') !== -1) {
                return 'fromCollection';
              }
              if (a.p.indexOf('#s2id_toCollections') !== -1) {
                return 'toCollection';
              }
            },
            attr: function () {
              if (a.p.indexOf('edgeD') !== -1) {
                return a.p;
              }
            },
            toArray: function () {
              if (a.p === '[id^=s2id_newEdgeDefinitions]') {
                return ['edgeD1', 'edgeD2', 'edgeD3'];
              }
            },
            val: function () {
              return 'newName';
            }
          }, g = {
            _key: 'name',
            name: 'name',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'collection2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: ['o1', 'o2', 'o3'],
            get: function (a) {
              return g[a];
            },
            deleteVertexCollection: function (o) {
              g.orphanCollections.splice(g.orphanCollections.indexOf(o), 1);
            },
            addVertexCollection: function (o) {
              g.orphanCollections.push(o);
            },
            addEdgeDefinition: function (o) {
              g.edgeDefinitions.push(o);
            },
            modifyEdgeDefinition: function (o) {},
            deleteEdgeDefinition: function (o) {
              g.edgeDefinitions.forEach(function (e) {
                if (e.collection === o.collection) {
                  delete g.edgeDefinitions[e];
                }
              });
            }
        };

        spyOn(_, 'pluck').andCallFake(function (s, b) {
          if (s === 'newVertexCollections') {
            return ['orphan1', 'orphan2', 'orphan3'];
          }
          if (s.indexOf('edgeD') !== -1) {
            return ['collection' + s.split('edgeD')[1]];
          }
          if (s === 'fromCollection') {
            return ['fromCollection'];
          }
          if (s === 'toCollection') {
            return ['toCollection'];
          }
        });

        spyOn(window, '$').andCallFake(function (x) {
          a.setParam(x);
          return a;
        });

        spyOn(view.collection, 'findWhere').andReturn(
          undefined
        );

        spyOn(view, 'updateGraphManagementView');

        spyOn(view.collection, 'create').andCallFake(function (a, b) {
          b.success();
        });
        spyOn(window.modalView, 'hide');
        view.createNewGraph();

        expect(view.updateGraphManagementView).toHaveBeenCalled();
      });

      it('should  saveCreatedGraph but return an error', function () {
        var a = {
            p: '',
            setParam: function (p) {
              a.p = p;
            },
            select2: function () {
              if (a.p === '#newVertexCollections') {
                return 'newVertexCollections';
              }
              if (a.p.indexOf('#s2id_newEdgeDefinitions') !== -1) {
                return a.p.split('#s2id_newEdgeDefinitions')[1];
              }
              if (a.p.indexOf('#s2id_fromCollections') !== -1) {
                return 'fromCollection';
              }
              if (a.p.indexOf('#s2id_toCollections') !== -1) {
                return 'toCollection';
              }
            },
            attr: function () {
              if (a.p.indexOf('edgeD') !== -1) {
                return a.p;
              }
            },
            toArray: function () {
              if (a.p === '[id^=s2id_newEdgeDefinitions]') {
                return ['edgeD1', 'edgeD2', 'edgeD3'];
              }
            },
            val: function () {
              return 'newName';
            }
          }, g = {
            _key: 'name',
            name: 'name',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'collection2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: ['o1', 'o2', 'o3'],
            get: function (a) {
              return g[a];
            },
            deleteVertexCollection: function (o) {
              g.orphanCollections.splice(g.orphanCollections.indexOf(o), 1);
            },
            addVertexCollection: function (o) {
              g.orphanCollections.push(o);
            },
            addEdgeDefinition: function (o) {
              g.edgeDefinitions.push(o);
            },
            modifyEdgeDefinition: function (o) {},
            deleteEdgeDefinition: function (o) {
              g.edgeDefinitions.forEach(function (e) {
                if (e.collection === o.collection) {
                  delete g.edgeDefinitions[e];
                }
              });
            }
        };

        spyOn(_, 'pluck').andCallFake(function (s, b) {
          if (s === 'newVertexCollections') {
            return ['orphan1', 'orphan2', 'orphan3'];
          }
          if (s.indexOf('edgeD') !== -1) {
            return ['collection' + s.split('edgeD')[1]];
          }
          if (s === 'fromCollection') {
            return ['fromCollection'];
          }
          if (s === 'toCollection') {
            return ['toCollection'];
          }
        });

        spyOn(window, '$').andCallFake(function (x) {
          a.setParam(x);
          return a;
        });

        spyOn(view.collection, 'findWhere').andReturn(
          undefined
        );

        spyOn(view, 'updateGraphManagementView');

        spyOn(view.collection, 'create').andCallFake(function (a, b) {
          b.error(a, {responseText: '{"errorMessage" : "blub"}'});
        });
        spyOn(window.modalView, 'hide');

        spyOn(arangoHelper, 'arangoError');

        view.createNewGraph();

        expect(arangoHelper.arangoError).toHaveBeenCalledWith(
          'blub');
        expect(view.updateGraphManagementView).not.toHaveBeenCalled();
      });

      it('should not saveCreatedGraph as name is missing', function () {
        var a = {
            p: '',
            setParam: function (p) {
              a.p = p;
            },
            select2: function () {
              if (a.p === '#newVertexCollections') {
                return 'newVertexCollections';
              }
              if (a.p.indexOf('#s2id_newEdgeDefinitions') !== -1) {
                return a.p.split('#s2id_newEdgeDefinitions')[1];
              }
              if (a.p.indexOf('#s2id_fromCollections') !== -1) {
                return 'fromCollection';
              }
              if (a.p.indexOf('#s2id_toCollections') !== -1) {
                return 'toCollection';
              }
            },
            attr: function () {
              if (a.p.indexOf('edgeD') !== -1) {
                return a.p;
              }
            },
            toArray: function () {
              if (a.p === '[id^=s2id_newEdgeDefinitions]') {
                return ['edgeD1', 'edgeD2', 'edgeD3'];
              }
            },
            val: function () {
              return undefined;
            }
          }, g = {
            _key: 'name',
            name: 'name',
            edgeDefinitions: [{
              collection: 'blub',
              from: ['bla'],
              to: ['blob']
            }, {
              collection: 'collection2',
              from: ['bla'],
              to: ['blob']
            }],
            orphanCollections: ['o1', 'o2', 'o3'],
            get: function (a) {
              return g[a];
            },
            deleteVertexCollection: function (o) {
              g.orphanCollections.splice(g.orphanCollections.indexOf(o), 1);
            },
            addVertexCollection: function (o) {
              g.orphanCollections.push(o);
            },
            addEdgeDefinition: function (o) {
              g.edgeDefinitions.push(o);
            },
            modifyEdgeDefinition: function (o) {},
            deleteEdgeDefinition: function (o) {
              g.edgeDefinitions.forEach(function (e) {
                if (e.collection === o.collection) {
                  delete g.edgeDefinitions[e];
                }
              });
            }
        };

        spyOn(_, 'pluck').andCallFake(function (s, b) {
          if (s === 'newVertexCollections') {
            return ['orphan1', 'orphan2', 'orphan3'];
          }
          if (s.indexOf('edgeD') !== -1) {
            return ['collection' + s.split('edgeD')[1]];
          }
          if (s === 'fromCollection') {
            return ['fromCollection'];
          }
          if (s === 'toCollection') {
            return ['toCollection'];
          }
        });

        spyOn(window, '$').andCallFake(function (x) {
          a.setParam(x);
          return a;
        });

        spyOn(view.collection, 'findWhere').andReturn(
          g
        );

        spyOn(arangoHelper, 'arangoError');

        view.createNewGraph();

        expect(arangoHelper.arangoError).toHaveBeenCalledWith(
          'A name for the graph has to be provided.');
      });
      describe('creating a new graph', function () {
        it('should create a new empty graph', function () {
          runs(function () {
            $('#createGraph').click();
          });
          waitsFor(function () {
            return $('#modal-dialog').css('display') === 'block';
          });
          runs(function () {
            $('#createNewGraphName').val('newGraph');
            $('#s2id_newEdgeDefinitions0').select2('val', ['newEdgeCol']);
            $('#s2id_fromCollections0').select2('val', ['newFrom1', 'newFrom2']);
            $('#s2id_toCollections0').select2('val', ['newTo1', 'newTo2']);
            $('#s2id_newVertexCollections').select2('val', ['newOrphan1', 'newOrphan2']);
            $('#newGraphEdges').val('newEdges');
            spyOn($, 'ajax').andCallFake(function (opts) {
              expect(opts.type).toEqual('POST');
              expect(opts.url).toEqual('/_api/gharial');
              expect(opts.data).toEqual(JSON.stringify(
                {
                  'name': 'newGraph',
                  'edgeDefinitions': [
                    {
                      collection: 'newEdgeCol',
                      from: ['newFrom1', 'newFrom2'],
                      to: ['newTo1', 'newTo2']
                    }
                  ],
                  'orphanCollections': ['newOrphan1', 'newOrphan2']
                }));
            });
            $('#modalButton1').click();
            expect($.ajax).toHaveBeenCalled();
          });
        });
      });
    });
  });
}());
