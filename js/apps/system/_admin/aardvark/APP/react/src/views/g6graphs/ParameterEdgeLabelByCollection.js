import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';

const ParameterEdgeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabelByCollection, setEdgeLabelByCollection] = useState(urlParameters.edgeLabelByCollection);

  const newUrlParameters = { ...urlParameters };
  
  return (
    <div style={{ 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Checkbox
        label='Show collection name'
        inline
        checked={edgeLabelByCollection}
        onChange={() => {
          const newEdgeLabelByCollection = !edgeLabelByCollection;
          setEdgeLabelByCollection(newEdgeLabelByCollection);
          newUrlParameters.edgeLabelByCollection = newEdgeLabelByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={'graphviewer'}
      />
      <ToolTip
        title={"Adds a collection name to the edge label."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterEdgeLabelByCollection;
