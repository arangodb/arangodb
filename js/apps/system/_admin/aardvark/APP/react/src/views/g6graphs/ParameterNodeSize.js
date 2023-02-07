import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{ marginBottom: '20px' }}>
      <Textinput
        label={'Sizing attribute'}
        value={nodeSize}
        width={'300px'}
        onChange={(e) => {
          setNodeSize(e.target.value);
          newUrlParameters.nodeSize = e.target.value;
          setUrlParameters(newUrlParameters);
        }}
        disabled={urlParameters.nodeSizeByEdges}>
      </Textinput>
      <ToolTip
        title={"If an attribute is given, nodes will be sized by the attribute."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterNodeSize;
