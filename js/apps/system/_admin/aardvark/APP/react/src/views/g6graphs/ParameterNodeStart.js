import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Node } from './components/node/node.component';
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeStart = ({ nodes, onNodeSelect }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeStart, setNodeStart] = useState(urlParameters.nodeStart);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
  <h5>urlParameters (in ParameterNodeStart): {JSON.stringify(urlParameters)}</h5>
      <p>nodeStart: {JSON.stringify(urlParameters.nodeStart)}</p>
      <p>depth: {JSON.stringify(urlParameters.depth)}</p>
      <p>limit: {JSON.stringify(urlParameters.limit)}</p>
      <p>nodeLabelByCollection: {JSON.stringify(urlParameters.nodeLabelByCollection)}</p>
      <p>edgeLabelByCollection: {JSON.stringify(urlParameters.edgeLabelByCollection)}</p>
      */

  return (
    <>
      <div>
      <label>
      <div className="node-list">
        <input
          list="nodelist"
          id="graphnodes"
          name="graphnodes"
          placeholder="Set start node"
          onChange={
            (e) => {
              setNodeStart(e.target.value);
              NEWURLPARAMETERS.nodeStart = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
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
        Start node
      </label>
    </div>
    </>
  );
};

export default ParameterNodeStart;
