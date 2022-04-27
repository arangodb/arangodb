import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterEdgeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabel, setEdgeLabel] = useState(urlParameters.edgeLabel);

  const NEWURLPARAMETERS = { ...urlParameters };  

  /*
  <h5>urlParameters (in ParameterEdgeLabel): {JSON.stringify(urlParameters)}</h5>
  */
  
  return (
    <>
      <form>
        <label>
          edgeLabel:
          <input
            type="text"
            value={edgeLabel}
            onChange={(e) => {
              setEdgeLabel(e.target.value);
              NEWURLPARAMETERS.edgeLabel = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterEdgeLabel;
