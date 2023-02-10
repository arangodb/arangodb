import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeColorAttribute = ({ nodesColorAttributes }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorAttribute, setNodeColorAttribute] = useState(urlParameters.nodeColorAttribute);

  const newUrlParameters = { ...urlParameters };

  return (
      <div style={{ 'marginTop': '24px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
        <Textinput
          label={'Node color attribute'}
          value={nodeColorAttribute}
          style={'graphviewer'}
          width={'200px'}
          onChange={(e) => {
            setNodeColorAttribute(e.target.value);
            newUrlParameters.nodeColorAttribute = e.target.value;
            setUrlParameters(newUrlParameters);
          }}
          disabled={urlParameters.nodeColorByCollection}>
        </Textinput>
        <ToolTip
          title={"If an attribute is given, nodes will be colorized by the attribute. This setting ignores default node color if set."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </div>
  );
};

export default ParameterNodeColorAttribute;
