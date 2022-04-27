import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabel, setNodeLabel] = useState(urlParameters.nodeLabel);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
  <h5>urlParameters (in ParameterNodeLabel): {JSON.stringify(urlParameters)}</h5>
  */
  
  return (
    <>
      <form>
        <label>
          nodeLabel:
          <input
            type="text"
            value={nodeLabel}
            onChange={(e) => {
              setNodeLabel(e.target.value);
              NEWURLPARAMETERS.nodeLabel = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterNodeLabel;
