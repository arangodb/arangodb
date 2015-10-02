/*jshint browser: true */
/*jshint unused: false */
/*jshint maxlen: 999999 */
/*global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, require, Viva*/

(function() {
  "use strict";

  window.GraphTestView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("graphTestView.ejs"),

    events: {
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      this.renderGraph();
    },

    renderGraph: function() {
      $('#cy').cytoscape({
        style: cytoscape.stylesheet()
        .selector('node')
        .css({
          'content': 'data(name)',
          'text-valign': 'center',
          'color': 'white',
          'text-outline-width': 2,
          'text-outline-color': '#888'
        })
        .selector('edge')
        .css({
          'target-arrow-shape': 'triangle'
        })
        .selector(':selected')
        .css({
          'background-color': 'black',
          'line-color': 'black',
          'target-arrow-color': 'black',
          'source-arrow-color': 'black'
        })
        .selector('.faded')
        .css({
          'opacity': 0.25,
          'text-opacity': 0
        }),

        elements: {
          nodes: [
            { data: { id: 'j', name: 'Jerry' } },
            { data: { id: 'e', name: 'Elaine' } },
            { data: { id: 'k', name: 'Kramer' } },
            { data: { id: 'g', name: 'George' } }
          ],
          edges: [
            { data: { source: 'j', target: 'e' } },
            { data: { source: 'j', target: 'k' } },
            { data: { source: 'j', target: 'g' } },
            { data: { source: 'e', target: 'j' } },
            { data: { source: 'e', target: 'k' } },
            { data: { source: 'k', target: 'j' } },
            { data: { source: 'k', target: 'e' } },
            { data: { source: 'k', target: 'g' } },
            { data: { source: 'g', target: 'j' } }
          ]
        },

        layout: {
          name: 'cola',
          padding: 10
        },

        ready: function(){
          window.cy = this;

          cy.elements().unselectify();

          cy.on('tap', 'node', function(e){
            var node = e.cyTarget; 
            var neighborhood = node.neighborhood().add(node);

            cy.elements().addClass('faded');
            neighborhood.removeClass('faded');
          });

          cy.on('tap', function(e){
            if( e.cyTarget === cy ){
              cy.elements().removeClass('faded');
            }
          });
        }
      });
    }

  });
}());

