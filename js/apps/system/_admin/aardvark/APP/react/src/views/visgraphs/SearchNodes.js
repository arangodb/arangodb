import React, { useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import { Node } from './components/node/Node';
import PlainLabel from "./components/pure-css/form/PlainLabel";

const SearchNodes = ({ nodes, graphData, data, onDeleteNode, onNodeSelect }) => {
  const [previousSearchedNode, setPreviousSearchedNode] = useState(null);

  return (
    <div style={{'marginTop': '24px'}}>
      <PlainLabel htmlFor={'searchnodes'}>Search for a node</PlainLabel>
      <div className="node-list">
        <input
          list="searchnodelist"
          id="searchnodes"
          name="searchnodes"
          onChange={
            (e) => {
              if(e.target.value.includes("/")) {
                onNodeSelect(previousSearchedNode, e.target.value);
                setPreviousSearchedNode(e.target.value);
                const element = document.getElementById("graph-card");
                element.scrollIntoView({ behavior: "smooth" });
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
        <datalist id="searchnodelist">
          {nodes.map(node => (
            <Node
              key={node.id}
              node={node} />
          ))}
        </datalist>
        <ToolTip
          title={"Select the node you are looking for."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
        </ToolTip>
      </div>
    </div>
  );
};

export default SearchNodes;
