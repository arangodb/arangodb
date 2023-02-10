import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterEdgeColorAttribute = ({ edgesColorAttributes }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorAttribute, setEdgeColorAttribute] = useState(urlParameters.edgeColorAttribute);

  const newUrlParameters = { ...urlParameters };
  
  return (
    <div style={{ 'marginTop': '24px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Textinput
        label={'Edge color attribute'}
        value={edgeColorAttribute}
        style={'graphviewer'}
        width={'300px'}
        onChange={(e) => {
          setEdgeColorAttribute(e.target.value);
          newUrlParameters.edgeColorAttribute = e.target.value;
          setUrlParameters(newUrlParameters);
        }}
        disabled={urlParameters.edgeColorByCollection}>
      </Textinput>
      <ToolTip
        title={"If an attribute is given, edges will be colorized by the attribute. This setting ignores default edge color if set."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterEdgeColorAttribute;
