import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeStart = ({ nodes, onNodeSelect }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeStart, setNodeStart] = useState(urlParameters.nodeStart);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{'marginTop': '24px'}}>
      <Textinput
        label={'Start node'}
        value={urlParameters.nodeStart}
        width={'500px'}
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
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterNodeStart;
