/* global arangoHelper, arangoFetch, frontendConfig */
import React, { useEffect, useState, useCallback, useRef } from 'react';
import { DeleteNodeModal } from './DeleteNodeModal';
import { message } from 'antd';

export const AddNodeToGraphData = ({ graphData, onAddNode }) => {
  const clearData = {
    collection: '',
    nodeId: ''
  };
  let [formData, setFormData] = useState(clearData);
  console.log("DeleteNodeFromGraphData (graphData): ", graphData);

  const addNode = (graphData) => {
    let nodes = graphData.nodes;
    const edges = graphData.edges;
    const settings = graphData.settings;
    console.log("AddNodeToGraphData - graphData.nodes: ", nodes);
    console.log("AddNodeToGraphData - graphData.edges: ", edges);
    console.log("AddNodeToGraphData - graphData.settings: ", settings);

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
    console.log("nodes with new node added: ", nodes);

    const mergedGraphData = {
      nodes,
      edges,
      settings
    };
    console.log("########################");
    console.log("AddNodeToGraphData() - mergedGraphData: ", mergedGraphData);
    setFormData(clearData);
    onAddNode(mergedGraphData);
    message.info(`Node "${formData.collection}/${formData.nodeId}" added`);
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
