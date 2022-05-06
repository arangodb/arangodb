import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterEdgeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabel, setEdgeLabel] = useState(urlParameters.edgeLabel);

  const NEWURLPARAMETERS = { ...urlParameters };  

  /*
  <>
      <form>
        <Textinput
          label={'Edge label'}
          value={edgeLabel}
          onChange={(e) => {
            setEdgeLabel(e.target.value);
            NEWURLPARAMETERS.edgeLabel = e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}>
        </Textinput>
      </form>
    </>
  */
  
  return (
    <div style={{'marginTop': '24px'}}>
        <Textinput
          label={'Edge label'}
          value={edgeLabel}
          width={'300px'}
          onChange={(e) => {
            setEdgeLabel(e.target.value);
            NEWURLPARAMETERS.edgeLabel = e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}>
        </Textinput>
        <Tooltip placement="bottom" title={"Node label. Please choose a valid and available node attribute."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </div>
  );
};

export default ParameterEdgeLabel;
