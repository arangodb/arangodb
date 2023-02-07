import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "../../components/pure-css/form/Checkbox";
import ToolTip from '../../components/arango/tootip';

const ParameterNodeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorByCollection, setNodeColorByCollection] = useState(urlParameters.nodeColorByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
      <Checkbox
        label='Color nodes by collection'
        inline
        checked={nodeColorByCollection}
        onChange={() => {
          const newNodeColorByCollection = !nodeColorByCollection;
          setNodeColorByCollection(newNodeColorByCollection);
          newUrlParameters.nodeColorByCollection = newNodeColorByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={{ 'color': '#736b68' }}
      />
      <ToolTip
        title={"Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px' }}></span>
      </ToolTip>
    </div>
  );
};

export default ParameterNodeColorByCollection;
