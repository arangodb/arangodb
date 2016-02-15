/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, sigma, templateEngine, document, _, $, arangoHelper, window*/

(function() {
  "use strict";
  window.testView = Backbone.View.extend({
    el: '#content',

    graph: {
      edges: [],
      nodes: []
    },

    events: {
    },

    initialize: function () {
      console.log(undefined);
    },

    template: templateEngine.createTemplate("testView.ejs"),

    render: function () {
      $(this.el).html(this.template.render({}));
      this.renderGraph();
      return this;
    },

    renderGraph: function () {

      this.convertData();

      console.log(this.graph);

      this.s = new sigma({
        graph: this.graph,
        container: 'graph-container',
        verbose: true,
        renderers: [
          {
            container: document.getElementById('graph-container'),
            type: 'webgl'
          }
        ]
      });
    },

    convertData: function () {

      var self = this;

      _.each(this.dump, function(value) {

        _.each(value.p, function(lol) {
          self.graph.nodes.push({
            id: lol.verticesvalue.v._id,
            label: value.v._key,
            x: Math.random(),
            y: Math.random(),
            size: Math.random()
          });

          self.graph.edges.push({
            id: value.e._id,
            source: value.e._from,
            target: value.e._to
          });
         

        });
      });

      return null;
    },
		
    dump: [
      {
        "v": {
          "label": "7",
          "_id": "circles/G",
          "_rev": "1841663870851",
          "_key": "G"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_foo",
          "_id": "edges/1841666099075",
          "_rev": "1841666099075",
          "_key": "1841666099075",
          "_from": "circles/A",
          "_to": "circles/G"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            }
          ]
        }
      },
      {
        "v": {
          "label": "8",
          "_id": "circles/H",
          "_rev": "1841664067459",
          "_key": "H"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_blob",
          "_id": "edges/1841666295683",
          "_rev": "1841666295683",
          "_key": "1841666295683",
          "_from": "circles/G",
          "_to": "circles/H"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "8",
              "_id": "circles/H",
              "_rev": "1841664067459",
              "_key": "H"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_blob",
              "_id": "edges/1841666295683",
              "_rev": "1841666295683",
              "_key": "1841666295683",
              "_from": "circles/G",
              "_to": "circles/H"
            }
          ]
        }
      },
      {
        "v": {
          "label": "9",
          "_id": "circles/I",
          "_rev": "1841664264067",
          "_key": "I"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_blub",
          "_id": "edges/1841666492291",
          "_rev": "1841666492291",
          "_key": "1841666492291",
          "_from": "circles/H",
          "_to": "circles/I"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "8",
              "_id": "circles/H",
              "_rev": "1841664067459",
              "_key": "H"
            },
            {
              "label": "9",
              "_id": "circles/I",
              "_rev": "1841664264067",
              "_key": "I"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_blob",
              "_id": "edges/1841666295683",
              "_rev": "1841666295683",
              "_key": "1841666295683",
              "_from": "circles/G",
              "_to": "circles/H"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_blub",
              "_id": "edges/1841666492291",
              "_rev": "1841666492291",
              "_key": "1841666492291",
              "_from": "circles/H",
              "_to": "circles/I"
            }
          ]
        }
      },
      {
        "v": {
          "label": "10",
          "_id": "circles/J",
          "_rev": "1841664460675",
          "_key": "J"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_zip",
          "_id": "edges/1841666688899",
          "_rev": "1841666688899",
          "_key": "1841666688899",
          "_from": "circles/G",
          "_to": "circles/J"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "10",
              "_id": "circles/J",
              "_rev": "1841664460675",
              "_key": "J"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_zip",
              "_id": "edges/1841666688899",
              "_rev": "1841666688899",
              "_key": "1841666688899",
              "_from": "circles/G",
              "_to": "circles/J"
            }
          ]
        }
      },
      {
        "v": {
          "label": "11",
          "_id": "circles/K",
          "_rev": "1841664657283",
          "_key": "K"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_zup",
          "_id": "edges/1841666885507",
          "_rev": "1841666885507",
          "_key": "1841666885507",
          "_from": "circles/J",
          "_to": "circles/K"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "10",
              "_id": "circles/J",
              "_rev": "1841664460675",
              "_key": "J"
            },
            {
              "label": "11",
              "_id": "circles/K",
              "_rev": "1841664657283",
              "_key": "K"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_zip",
              "_id": "edges/1841666688899",
              "_rev": "1841666688899",
              "_key": "1841666688899",
              "_from": "circles/G",
              "_to": "circles/J"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_zup",
              "_id": "edges/1841666885507",
              "_rev": "1841666885507",
              "_key": "1841666885507",
              "_from": "circles/J",
              "_to": "circles/K"
            }
          ]
        }
      },
      {
        "v": {
          "label": "2",
          "_id": "circles/B",
          "_rev": "1841662887811",
          "_key": "B"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_bar",
          "_id": "edges/1841665116035",
          "_rev": "1841665116035",
          "_key": "1841665116035",
          "_from": "circles/A",
          "_to": "circles/B"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            }
          ]
        }
      },
      {
        "v": {
          "label": "5",
          "_id": "circles/E",
          "_rev": "1841663477635",
          "_key": "E"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_blub",
          "_id": "edges/1841665705859",
          "_rev": "1841665705859",
          "_key": "1841665705859",
          "_from": "circles/B",
          "_to": "circles/E"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "5",
              "_id": "circles/E",
              "_rev": "1841663477635",
              "_key": "E"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blub",
              "_id": "edges/1841665705859",
              "_rev": "1841665705859",
              "_key": "1841665705859",
              "_from": "circles/B",
              "_to": "circles/E"
            }
          ]
        }
      },
      {
        "v": {
          "label": "6",
          "_id": "circles/F",
          "_rev": "1841663674243",
          "_key": "F"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_schubi",
          "_id": "edges/1841665902467",
          "_rev": "1841665902467",
          "_key": "1841665902467",
          "_from": "circles/E",
          "_to": "circles/F"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "5",
              "_id": "circles/E",
              "_rev": "1841663477635",
              "_key": "E"
            },
            {
              "label": "6",
              "_id": "circles/F",
              "_rev": "1841663674243",
              "_key": "F"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blub",
              "_id": "edges/1841665705859",
              "_rev": "1841665705859",
              "_key": "1841665705859",
              "_from": "circles/B",
              "_to": "circles/E"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_schubi",
              "_id": "edges/1841665902467",
              "_rev": "1841665902467",
              "_key": "1841665902467",
              "_from": "circles/E",
              "_to": "circles/F"
            }
          ]
        }
      },
      {
        "v": {
          "label": "3",
          "_id": "circles/C",
          "_rev": "1841663084419",
          "_key": "C"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_blarg",
          "_id": "edges/1841665312643",
          "_rev": "1841665312643",
          "_key": "1841665312643",
          "_from": "circles/B",
          "_to": "circles/C"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "3",
              "_id": "circles/C",
              "_rev": "1841663084419",
              "_key": "C"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blarg",
              "_id": "edges/1841665312643",
              "_rev": "1841665312643",
              "_key": "1841665312643",
              "_from": "circles/B",
              "_to": "circles/C"
            }
          ]
        }
      },
      {
        "v": {
          "label": "4",
          "_id": "circles/D",
          "_rev": "1841663281027",
          "_key": "D"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_blorg",
          "_id": "edges/1841665509251",
          "_rev": "1841665509251",
          "_key": "1841665509251",
          "_from": "circles/C",
          "_to": "circles/D"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "3",
              "_id": "circles/C",
              "_rev": "1841663084419",
              "_key": "C"
            },
            {
              "label": "4",
              "_id": "circles/D",
              "_rev": "1841663281027",
              "_key": "D"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blarg",
              "_id": "edges/1841665312643",
              "_rev": "1841665312643",
              "_key": "1841665312643",
              "_from": "circles/B",
              "_to": "circles/C"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blorg",
              "_id": "edges/1841665509251",
              "_rev": "1841665509251",
              "_key": "1841665509251",
              "_from": "circles/C",
              "_to": "circles/D"
            }
          ]
        }
      }
    ],

  });
}());
