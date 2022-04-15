import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Switch, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeSizeByEdges = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSizeByEdges, setNodeSizeByEdges] = useState(urlParameters.nodeSizeByEdges);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <label>
        <input
          type="checkbox"
          checked={nodeSizeByEdges}
          onChange={() => {
            const newNodeSizeByEdges = !nodeSizeByEdges;
            setNodeSizeByEdges(newNodeSizeByEdges);
            NEWURLPARAMETERS.nodeSizeByEdges = newNodeSizeByEdges;
            setUrlParameters(NEWURLPARAMETERS);
          }}
        />
        Size by connections
      </label>
      <p>Do we calculate the node size via connections? {nodeSizeByEdges.toString()}</p>
      <Tooltip title="Append collection name to the label?">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
      </Tooltip>
    </>
  );
};

export default ParameterNodeSizeByEdges;
