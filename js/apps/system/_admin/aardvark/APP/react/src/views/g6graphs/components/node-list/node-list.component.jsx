import React from 'react';

import { Node } from '../node/node.component';

export const NodeList = ({ nodes, graphData, onNodeInfo, data, onDeleteNode, onNodeSelect }) => {

  return (
    <div>
      <div className="node-list">
        <input
          list="nodelist"
          id="graphnodes"
          name="graphnodes"
          placeholder="Search nodes..."
          onChange={
            (e) => {
              onNodeSelect(e.target.value);
            }
          }
          style={{ 'width': '100%' }} />
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
