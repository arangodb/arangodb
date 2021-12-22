import React from 'react';

import { Edge } from '../edge/edge.component';

export const EdgeList = ({ edges }) => (
  <div>
    <div className='edge-list'>
      <input list="edgelist" id="graphedges" name="graphedges" placeholder="Search edges..." style={{ 'width': '90%' }} />
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
