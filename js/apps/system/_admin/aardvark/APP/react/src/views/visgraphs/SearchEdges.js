import React, { useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import { Edge } from './components/edge/Edge';
import PlainLabel from "./components/pure-css/form/PlainLabel";

const SearchEdges = ({ edges, graphData, onEdgeInfo, data, onDeleteEdge, onEdgeSelect }) => {
  const [previousSearchedEdge, setPreviousSearchedEdge] = useState(null);
  
  return (
    <div style={{'marginTop': '24px', marginBottom: '20px'}}>
      <PlainLabel htmlFor={'searchedges'}>Search for an edge</PlainLabel>
      <div className="edge-list">
        <input
          list="searchedgelist"
          id="searchedges"
          name="searchedges"
          onChange={
            (e) => {
              if(e.target.value.includes("/")) {
                onEdgeSelect(previousSearchedEdge, e.target.value);
                setPreviousSearchedEdge(e.target.value);
              }
            }
          }
          style={{
            'width': '500px',
            'height': 'auto',
            'margin-right': '8px',
            'color': '#555555',
            'border': '2px solid rgba(140, 138, 137, 0.25)',
            'border-radius': '4px',
            'background-color': '#fff !important',
            'box-shadow': 'none',
            'outline': 'none',
            'outline-color': 'transparent',
            'outline-style': 'none'
          }} />
        <datalist id="searchedgelist">
          {edges.map(edge => (
            <Edge
              key={edge.id}
              edge={edge} />
          ))}
        </datalist>
        <ToolTip
          title={"Select the edge you are looking for."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
        </ToolTip>
      </div>
    </div>
  );
};

export default SearchEdges;
