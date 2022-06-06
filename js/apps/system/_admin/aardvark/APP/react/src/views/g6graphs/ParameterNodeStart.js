import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Node } from './components/node/node.component';
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import PlainLabel from "./components/pure-css/form/PlainLabel";

const ParameterNodeStart = ({ nodes, onNodeSelect }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeStart, setNodeStart] = useState(urlParameters.nodeStart);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <div style={{'marginTop': '24px'}}>
      <PlainLabel htmlFor={'graphnodes'}>Start node</PlainLabel>
      <div className="node-list">
        <input
          list="nodelist"
          id="graphnodes"
          name="graphnodes"
          autocomplete="off"
          onChange={
            (e) => {
              setNodeStart(e.target.value);
              NEWURLPARAMETERS.nodeStart = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
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
          }}
        />
        <datalist id="nodelist">
          {nodes.map(node => (
            <Node
              key={node.id}
              node={node} />
          ))}
        </datalist>
        <Tooltip placement="bottom" title={"A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </div>
    </div>
  );
};

export default ParameterNodeStart;
