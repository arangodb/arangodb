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
      <Tooltip placement="bottom" title={"Different graph algorithms. No overlap is very fast (more than 5000 nodes), force is slower (less than 5000 nodes) and fruchtermann is the slowest (less than 500 nodes)."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeLabelByCollection;
