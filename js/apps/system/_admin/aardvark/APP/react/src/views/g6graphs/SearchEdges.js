import React from 'react';
import { Tooltip } from 'antd';
import { Edge } from './components/edge/edge.component';
import { InfoCircleFilled } from '@ant-design/icons';
import PlainLabel from "./components/pure-css/form/PlainLabel";

const SearchEdges = ({ edges, graphData, onEdgeInfo, data, onDeleteEdge, onEdgeSelect }) => {

  return (
    <div style={{'marginTop': '24px', marginBottom: '20px'}}>
      <PlainLabel htmlFor={'searchedges'}>Search edge</PlainLabel>
      <div className="edge-list">
        <input
          list="searchedgelist"
          id="searchedges"
          name="searchedges"
          onChange={
            (e) => {
              if(e.target.value.includes("/")) {
                console.log("e.target.value (in SearchEdges): ", e.target.value);
                onEdgeSelect(e.target.value);
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
        <Tooltip placement="bottom" title={"Select the edge you are looking for."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </div>
    </div>
  );
};

export default SearchEdges;
