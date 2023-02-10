import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterEdgeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabel, setEdgeLabel] = useState(urlParameters.edgeLabel);

  const newUrlParameters = { ...urlParameters };  
  
  return (
    <div style={{ 'marginTop': '24px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Textinput
        label={'Edge label'}
        value={edgeLabel}
        style={'graphviewer'}
        width={'200px'}
        onChange={(e) => {
          setEdgeLabel(e.target.value);
          newUrlParameters.edgeLabel = e.target.value;
          setUrlParameters(newUrlParameters);
        }}>
      </Textinput>
      <ToolTip
        title={"Enter a valid edge attribute to be used as an edge label."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterEdgeLabel;
