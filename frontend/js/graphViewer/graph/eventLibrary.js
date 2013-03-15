/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/* global eventLibrary */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// Example onclick = new eventLibrary.Expand(edges, nodes, start, adapter.loadNode, nodeShaper.reshapeNode)
function EventLibrary() {
  this.Expand = function (edges, nodes, startCallback, loadNode, reshapeNode) {
    if (edges === undefined) {
      throw "Edges have to be defined";
    }
  
    if (nodes === undefined) {
      throw "Nodes have to be defined";
    }
  
    if (startCallback === undefined) {
      throw "A callback to the Start-method has to be defined";
    }
  
    if (loadNode === undefined) {
      throw "A callback to load a node has to be defined";
    }
  
    if (reshapeNode === undefined) {
      throw "A callback to reshape a node has to be defined";
    }
  
    var removeNode = function (node) {
      var i;
      for ( i = 0; i < nodes.length; i++ ) {
        if ( nodes[i] === node ) {
          nodes.splice( i, 1 );
          return;
        }
      }
    },
  
    // Helper function to easily remove all outbound edges for one node
    removeOutboundEdgesFromNode = function ( node ) {
      if (node._outboundCounter > 0) {
        var subNodes = [];
        var i;
        for ( i = 0; i < edges.length; i++ ) {
          if ( edges[i].source === node ) {
            subNodes.push(edges[i].target);
            node._outboundCounter--;
            edges[i].target._inboundCounter--;
            edges.splice( i, 1 );
            if (node._outboundCounter === 0) {
              break;
            }
            i--;
          }
        }
        return subNodes;
      }
    },
  
    collapseNode = function(node) {
      node._expanded = false;
      var subNodes = removeOutboundEdgesFromNode(node);    
      _.each(subNodes, function (n) {
        if (n._inboundCounter === 0) {
          collapseNode(n);
          removeNode(n);
        }
      });
    },
  
    expandNode = function(n) {
      n._expanded = true;
      loadNode(n.id, startCallback);
    };
  
    // @ before layouter.stop()
    // nodeClicked = function(n)
    return function(n) {
      if (!n._expanded) {
        expandNode(n);
      } else {
        collapseNode(n);
      }
      reshapeNode(n);
      startCallback();
    };
  };
}


