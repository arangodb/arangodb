import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterEdgeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColor, setEdgeColor] = useState(urlParameters.edgeColor);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <form>
        <label>
          Color:
          <input
            type="color"
            value={edgeColor}
            onChange={(e) => {
              setEdgeColor(e.target.value);
              NEWURLPARAMETERS.edgeColor = e.target.value.replace("#", "");
              setUrlParameters(NEWURLPARAMETERS);
            }}
            style={{ height: "40px" }}
            disabled={urlParameters.edgeColorByCollection}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterEdgeColor;
