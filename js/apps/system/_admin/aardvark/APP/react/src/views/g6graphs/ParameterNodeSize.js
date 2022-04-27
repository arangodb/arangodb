import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <form>
        <label>
          Sizing attribute:
          <input
            type="text"
            value={nodeSize}
            onChange={(e) => {
              setNodeSize(e.target.value);
              NEWURLPARAMETERS.nodeSize = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
            disabled={urlParameters.nodeSizeByEdges}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterNodeSize;
