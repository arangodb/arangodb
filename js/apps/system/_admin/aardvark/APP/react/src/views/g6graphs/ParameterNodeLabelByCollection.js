import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterNodeLabelByCollection = ({ graphData, onAddCollectionNameChange }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(urlParameters.nodeLabelByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
  // add collection name to nodes on the client side
  const addCollectionToNodeLabel = ({onAddCollectionNameChange}) => {
    const nodes = graphData.nodes;
    console.log("nodes in blablablubb: ", nodes);
    
    nodes.forEach((node) => {
      console.log("node: ", node);
      const idSplit = node.id.split('/');

      const model = {
        label: `${idSplit[1]} (${idSplit[0]})`
      };
      this.graph.updateItem(node, model);
    });
  }

  <input
          type="checkbox"
          checked={nodeLabelByCollection}
          onChange={() => {
            const newNodeLabelByCollection = !nodeLabelByCollection;
            setNodeLabelByCollection(newNodeLabelByCollection);
            NEWURLPARAMETERS.nodeLabelByCollection = newNodeLabelByCollection;
            setUrlParameters(NEWURLPARAMETERS);
            onAddCollectionNameChange(newNodeLabelByCollection);
          }}
        />
  */

  return (
    <div>
      <Checkbox
        checked={nodeLabelByCollection}
        onChange={() => {
          const newNodeLabelByCollection = !nodeLabelByCollection;
          setNodeLabelByCollection(newNodeLabelByCollection);
          NEWURLPARAMETERS.nodeLabelByCollection = newNodeLabelByCollection;
          setUrlParameters(NEWURLPARAMETERS);
          onAddCollectionNameChange(newNodeLabelByCollection);
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
