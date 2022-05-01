import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';
import Textinput from "../../components/pure-css/form/Textinput";

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
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
          />
        </label>
  */

  return (
    <>
      <form>
        <Textinput
          label={'Sizing attribute'}
          value={nodeSize}
          onChange={(e) => {
            setNodeSize(e.target.value);
            NEWURLPARAMETERS.nodeSize = e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}
          disabled={urlParameters.nodeSizeByEdges}>
        </Textinput>
      </form>
    </>
  );
};

export default ParameterNodeSize;
