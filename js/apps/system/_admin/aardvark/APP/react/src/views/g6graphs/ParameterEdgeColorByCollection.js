import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(urlParameters.edgeColorByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{ 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Checkbox
        label='Color edges by collection'
        inline
        checked={edgeColorByCollection}
        onChange={() => {
          const newEdgeColorByCollection = !edgeColorByCollection;
          setEdgeColorByCollection(newEdgeColorByCollection);
          newUrlParameters.edgeColorByCollection = newEdgeColorByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={'graphviewer'}
      />
      <ToolTip
        title={"Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterEdgeColorByCollection;
