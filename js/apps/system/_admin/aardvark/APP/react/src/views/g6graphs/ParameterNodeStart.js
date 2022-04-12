import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeStart = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeStart, setNodeStart] = useState(urlParameters.nodeStart);

  const NEWURLPARAMETERS = { ...urlParameters };
  
  return (
    <>
      <h5>urlParameters (in ParameterNodeStart): {JSON.stringify(urlParameters)}</h5>
      <form>
        <label>
          nodeStart:
          <input
            type="text"
            value={nodeStart}
            onChange={(e) => {
              setNodeStart(e.target.value);
              NEWURLPARAMETERS.nodeStart = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterNodeStart;
