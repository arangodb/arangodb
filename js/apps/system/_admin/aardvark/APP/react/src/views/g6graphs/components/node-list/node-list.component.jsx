import React from 'react';

import { Node } from '../node/node.component';

export const NodeList = ({ nodes, graphData, onNodeInfo, data, onDeleteNode }) => {
  console.log('nodes (NodeList): ', nodes);
  console.log('graphData (NodeList): ', graphData);
  console.log('onNodeInfo (NodeList): ', onNodeInfo);

  /*
  const onDeleteNode = (node) => {
    console.log('key in onDeleteNode in nodelist: ', node);
  };
  */

  const onNodeEdit = ( node ) => {
    console.log("Edit in NODELIST-Componet (node): ", node);
    console.log("Edit in NODELIST-Componet (graphData): ", graphData);
  }

  return (
    <div>
      <div className="node-list">
        <input list="nodelist" id="graphnodes" name="graphnodes" placeholder="Search nodes..." style={{ 'width': '90%' }} />
        <datalist id="nodelist">
          {nodes.map(node => (
            <Node
              key={node.id}
              node={node} />
          ))}
        </datalist>
      </div>
    </div>
  );
}
