import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const NEWURLPARAMETERS = { ...urlParameters };
  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith('#')) {
    calculatedNodeColor = '#' + nodeColor;
  }

  return (
    <>
      <form>
        <label>
          Color:
          <input
            type="color"
            value={calculatedNodeColor}
            onChange={(e) => {
              setNodeColor(e.target.value);
              NEWURLPARAMETERS.nodeColor = e.target.value.replace("#", "");
              setUrlParameters(NEWURLPARAMETERS);
            }}
            style={{ height: "40px" }}
            disabled={urlParameters.nodeColorByCollection}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterNodeColor;
