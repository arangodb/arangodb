import React from 'react';

import { Edge } from '../edge/edge.component';

export const EdgeList = ({ edges, graphData, onEdgeInfo, data, onDeleteEdge, onEdgeSelect }) => (
  <div>
    <div className='edge-list'>
      <input
        list="edgelist"
        id="graphedges"
        name="graphedges"
        placeholder="Search edges..."
        onChange={
          (e) => {
            onEdgeSelect(e.target.value);
          }
        }
        style={{ 'width': '100%', 'marginBottom': '24px' }} />
      <datalist id="edgelist">
        {edges.map(edge => (
          <Edge
            key={edge.id}
            edge={edge} />
        ))}
      </datalist>
    </div>
  </div>
);
