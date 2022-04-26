import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <form>
        <label>
          Color:
          <input
            type="color"
            value={nodeColor}
            onChange={(e) => {
              setNodeColor(e.target.value);
              NEWURLPARAMETERS.nodeColor = e.target.value.replace("#", "");
              setUrlParameters(NEWURLPARAMETERS);
            }}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterNodeColor;
