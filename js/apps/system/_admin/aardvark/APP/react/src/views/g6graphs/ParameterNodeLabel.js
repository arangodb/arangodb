import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabel, setNodeLabel] = useState(urlParameters.nodeLabel);

  const NEWURLPARAMETERS = { ...urlParameters };
  
  return (
    <>
      <div style={{'marginTop': '24px'}}>
        <Textinput
          label={'Node label'}
          value={nodeLabel}
          width={'300px'}
          onChange={(e) => {
            setNodeLabel(e.target.value);
            NEWURLPARAMETERS.nodeLabel = e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}>
        </Textinput>
        <Tooltip placement="bottom" title={"Enter a valid node attribute to be used as a node label."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </div>
    </>
  );
};

export default ParameterNodeLabel;
