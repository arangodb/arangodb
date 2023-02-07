import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "../../components/pure-css/form/Checkbox";
import ToolTip from '../../components/arango/tootip';

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(urlParameters.edgeColorByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
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
        style={{ 'color': '#736b68' }}
      />
      <ToolTip
        title={"Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterEdgeColorByCollection;
