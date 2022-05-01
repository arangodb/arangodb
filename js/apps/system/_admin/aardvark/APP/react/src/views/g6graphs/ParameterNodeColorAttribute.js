import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';
import Textinput from "../../components/pure-css/form/Textinput";

const ParameterNodeColorAttribute = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorAttribute, setNodeColorAttribute] = useState(urlParameters.nodeColorAttribute);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
  <label>
          Node color attribute:
          <input
            type="text"
            value={nodeColorAttribute}
            onChange={(e) => {
              setNodeColorAttribute(e.target.value);
              NEWURLPARAMETERS.nodeColorAttribute = e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
            disabled={urlParameters.nodeColorByCollection}
          />
        </label>
  */
  
  return (
    <>
      <form>
        <Textinput
          label={'Node color attribute'}
          value={nodeColorAttribute}
          onChange={(e) => {
            setNodeColorAttribute(e.target.value);
            NEWURLPARAMETERS.nodeColorAttribute = e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}>
        </Textinput>
      </form>
    </>
  );
};

export default ParameterNodeColorAttribute;
