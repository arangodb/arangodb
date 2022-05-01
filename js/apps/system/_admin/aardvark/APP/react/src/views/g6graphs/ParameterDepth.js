import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';
import Textinput from "../../components/pure-css/form/Textinput";

const ParameterDepth = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [depth, setDepth] = useState(urlParameters.depth);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
  <h5>urlParameters (context value): {JSON.stringify(urlParameters)}</h5>
  <label>
          Depth:
          <input
            type="text"
            value={depth}
            onChange={(e) => {
              setDepth(+e.target.value);
              NEWURLPARAMETERS.depth = +e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
          />
        </label>
  */
  
  return (
    <>
      <form>
        <Textinput
          label={'Depth'}
          value={depth}
          onChange={(e) => {
            setDepth(+e.target.value);
            NEWURLPARAMETERS.depth = +e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}>
        </Textinput>
      </form>
    </>
  );
};

export default ParameterDepth;
