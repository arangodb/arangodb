import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Switch, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeLabelByCollection = ({ graphData, onAddCollectionNameChange }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(urlParameters.nodeLabelByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };

  const addCollectionToNodeLabel = ({onAddCollectionNameChange}) => {
    const nodes = graphData.nodes;
    console.log("nodes in blablablubb: ", nodes);
    /*
    nodes.forEach((node) => {
      console.log("node: ", node);
      const idSplit = node._cfg.id.split('/');

      const model = {
        label: `${idSplit[1]} (${idSplit[0]})`
      };
      this.graph.updateItem(node, model);
    });
    */
  }

  return (
    <>
      <button onClick={addCollectionToNodeLabel}>Add collection to nodes</button>
      <label>
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
        Show collection name
      </label>
      <p>Do we show the collection name? {nodeLabelByCollection.toString()}</p>
      <Tooltip title="Append collection name to the label?">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
      </Tooltip>
    </>
  );
};

export default ParameterNodeLabelByCollection;
