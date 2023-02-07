import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "../../components/pure-css/form/Checkbox";
import ToolTip from '../../components/arango/tootip';

const ParameterNodeLabelByCollection = ({ graphData, onAddCollectionNameChange }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(urlParameters.nodeLabelByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
      <Checkbox
        label='Show collection name'
        inline
        checked={nodeLabelByCollection}
        onChange={() => {
          const newNodeLabelByCollection = !nodeLabelByCollection;
          setNodeLabelByCollection(newNodeLabelByCollection);
          newUrlParameters.nodeLabelByCollection = newNodeLabelByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={{ 'color': '#736b68' }}
      />
      <ToolTip
        title={"Adds a collection name to the node label."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterNodeLabelByCollection;
