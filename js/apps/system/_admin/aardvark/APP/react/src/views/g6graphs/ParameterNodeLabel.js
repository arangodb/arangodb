import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabel, setNodeLabel] = useState(urlParameters.nodeLabel);

  const newUrlParameters = { ...urlParameters };
  
  return (
    <>
      <div style={{'marginTop': '24px'}}>
        <Textinput
          label={'Node label'}
          value={nodeLabel}
          width={'300px'}
          onChange={(e) => {
            setNodeLabel(e.target.value);
            newUrlParameters.nodeLabel = e.target.value;
            setUrlParameters(newUrlParameters);
          }}>
        </Textinput>
        <ToolTip
          title={"Enter a valid node attribute to be used as a node label."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
        </ToolTip>
      </div>
    </>
  );
};

export default ParameterNodeLabel;
