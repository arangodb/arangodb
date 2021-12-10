import React from 'react';

import { Node } from '../node/node.component';

export const NodeList = ({ nodes, onNodeInfo }) => {

  const onNodeEdit2 = ( node ) => {
    console.log("Edit in NODELIST-COmponet: ", node);
  }

  return (
    <div>
      <div className="node-list">
        <label for="graphnodes">Search nodes:</label>
        <input list="nodelist" id="graphnodes" name="graphnodes" />
        <datalist id="nodelist">
          {nodes.map(node => (
            <Node
              key={node.id}
              node={node}
              onNodeInfo={onNodeInfo}
              onNodeEdit={onNodeEdit2} />
          ))}
        </datalist>
      </div>
    </div>
  );
}
