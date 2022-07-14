import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterNodeLabelByCollection = ({ graphData, onAddCollectionNameChange }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(urlParameters.nodeLabelByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
      <Checkbox
        checked={nodeLabelByCollection}
        onChange={() => {
          const newNodeLabelByCollection = !nodeLabelByCollection;
          setNodeLabelByCollection(newNodeLabelByCollection);
          newUrlParameters.nodeLabelByCollection = newNodeLabelByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={{ 'color': '#736b68' }}
      >
        Show collection name
      </Checkbox>
      <Tooltip placement="bottom" title={"Adds a collection name to the node label."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeLabelByCollection;
