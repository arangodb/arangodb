import React from 'react';

import { Edge } from '../edge/edge.component';

export const EdgeList = ({ edges }) => (
  <div>
    <div className='edge-list'>
      <label for="graphedges">Search edges:</label>
      <input list="edgelist" id="graphedges" name="graphedges" />
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
