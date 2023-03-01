import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeStart = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeStart, setNodeStart] = useState(urlParameters.nodeStart);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{ 'marginTop': '24px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Textinput
        label={'Start node'}
        value={urlParameters.nodeStart}
        template={'graphviewer'}
        width={'200px'}
        onChange={(e) => {
          setNodeStart(e.target.value);
          newUrlParameters.nodeStart = e.target.value;
          setUrlParameters(newUrlParameters);
        }}>
      </Textinput>
      <ToolTip
        title={"A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterNodeStart;
