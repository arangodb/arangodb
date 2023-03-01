/* global arangoHelper, arangoFetch, frontendConfig */
import React, { useState } from 'react';

export const AddNodeToGraphData = ({ graphData, onAddNode }) => {
  const clearData = {
    collection: '',
    nodeId: ''
  };
  let [formData, setFormData] = useState(clearData);

  const addNode = (graphData) => {
    let nodes = graphData.nodes;
    const edges = graphData.edges;
    const settings = graphData.settings;

    // add new node
    const newNode = {
      "id": `${formData.collection}/${formData.nodeId}`,
      "label": `${formData.nodeId}`,
      "style": {
        "label": {
            "value": `${formData.nodeId}`
        }
      }
    };
    nodes.push(newNode);

    const mergedGraphData = {
      nodes,
      edges,
      settings
    };
    setFormData(clearData);
    onAddNode(mergedGraphData);
  }

  return (
    <>
      <input
        type="text"
        onChange={(event) => { setFormData({ ...formData, collection: event.target.value})}}
        value={formData.collection}
        placeholder="collection name"
        style={{width: '90%'}} /><br />
      <input
        type="text"
        onChange={(event) => { setFormData({ ...formData, nodeId: event.target.value})}}
        value={formData.nodeId}
        placeholder="node key"
        style={{width: '90%'}} /><br />
      <button
        onClick={() => { addNode(graphData) }}
        className="button-primary">
        Add node
      </button>
    </>
  );
}
