/* global arangoHelper, arangoFetch, frontendConfig */
import React, { useEffect, useState, useCallback, useRef } from 'react';
import { DeleteNodeModal } from './DeleteNodeModal';
import { message } from 'antd';

export const AddEdgeToGraphData = ({ graphData, onAddEdge }) => {
  const clearData = {
    collection: '',
    sourceId: '',
    targetId: ''
  };
  let [formData, setFormData] = useState(clearData);

  const addEdge = (graphData) => {
    const nodes = graphData.nodes;
    let edges = graphData.edges;
    const settings = graphData.settings;
    const randomNumber = Math.floor(Math.random() * 10000) + 50000;

    // add new edge
    const newEdge = {
      "id": `${formData.collection}/${randomNumber}`,
      "source": `${formData.sourceId}`,
      "label": "",
      "color": "#cccccc",
      "target": `${formData.targetId}`,
      "size": 1,
      "sortColor": "#cccccc"
    };
    console.log("new edge: ", newEdge);
    edges.push(newEdge);
    console.log("edges with new edge added: ", edges);

    const mergedGraphData = {
      nodes,
      edges,
      settings
    };
    console.log("########################");
    console.log("AddEdgeToGraphData() - mergedGraphData: ", mergedGraphData);
    setFormData(clearData);
    onAddEdge(mergedGraphData);
    message.info(`Edge from "${formData.sourceId}" to "${formData.targetId}" added`);
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
        onChange={(event) => { setFormData({ ...formData, sourceId: event.target.value})}}
        value={formData.sourceId}
        placeholder="source id"
        style={{width: '90%'}} /><br />
      <input
        type="text"
        onChange={(event) => { setFormData({ ...formData, targetId: event.target.value})}}
        value={formData.targetId}
        placeholder="target id"
        style={{width: '90%'}} /><br />
      <button
        onClick={() => { addEdge(graphData) }}
        className="button-primary">
        Add edge
      </button>
    </>
  );
}
