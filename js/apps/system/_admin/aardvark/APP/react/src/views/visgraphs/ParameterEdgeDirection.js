import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';

const ParameterEdgeDirection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeDirection, setEdgeDirection] = useState(urlParameters.edgeDirection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{ 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Checkbox
        label='Show edge direction'
        inline
        checked={edgeDirection}
        onChange={() => {
          const newEdgeDirection = !edgeDirection;
          setEdgeDirection(newEdgeDirection);
          newUrlParameters.edgeDirection = newEdgeDirection;
          setUrlParameters(newUrlParameters);
        }}
        template={'graphviewer'}
      />
      <ToolTip
        title={"When true, an arrowhead on the 'to' side of the edge is drawn."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterEdgeDirection;
