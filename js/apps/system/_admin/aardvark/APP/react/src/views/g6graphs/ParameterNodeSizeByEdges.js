import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';

const ParameterNodeSizeByEdges = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSizeByEdges, setNodeSizeByEdges] = useState(urlParameters.nodeSizeByEdges);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{ 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Checkbox
        label='Size by connections'
        inline
        checked={nodeSizeByEdges}
        onChange={() => {
          const newNodeSizeByEdges = !nodeSizeByEdges;
          setNodeSizeByEdges(newNodeSizeByEdges);
          newUrlParameters.nodeSizeByEdges = newNodeSizeByEdges;
          setUrlParameters(newUrlParameters);
        }}
        style={'graphviewer'}
      />
      <ToolTip
        title={"If enabled, nodes are sized according to the number of edges they have. This option overrides the sizing attribute."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterNodeSizeByEdges;
