/* global arangoHelper, arangoFetch, frontendConfig */
import React, { useEffect, useState, useCallback, useRef } from 'react';
import { DeleteNodeModal } from './DeleteNodeModal';

export const DeleteNodeFromGraphData = ({ graphData, deleteNodeId, onDeleteNode }) => {
  console.log("DeleteNodeFromGraphData (graphData): ", graphData);

  const deleteNode = (graphData, deleteNodeId) => {
    console.log("DeleteNodeFromGraphData in deleteNode (graphData): ", graphData);
    console.log("DeleteNodeFromGraphData in deleteNode (deleteNodeId): ", deleteNodeId);

    console.log("DeleteNodeFromGraphData() - graphData.nodes: ", graphData.nodes);
    console.log("DeleteNodeFromGraphData() - graphData.edges: ", graphData.edges);
    console.log("DeleteNodeFromGraphData() - graphData.settings: ", graphData.settings);

    // delete node from nodes
    const nodes = graphData.nodes.filter((node) => {
      return node.id !== deleteNodeId;
    });

    // delete edges with deleteNodeId as source
    let edges = graphData.edges.filter((edge) => {
      return edge.source !== deleteNodeId;
    });

    // delete edges with deleteNodeId as target
    edges = edges.filter((edge) => {
      return edge.target !== deleteNodeId;
    });
   
    //const edges = graphData.edges;
    const settings = graphData.settings;
    const mergedGraphData = {
      nodes,
      edges,
      settings
    };
    console.log("########################");
    console.log("DeleteNodeFromGraphData() - nodes: ", nodes);
    console.log("DeleteNodeFromGraphData() - edges: ", edges);
    //console.log("DeleteNodeFromGraphData() - newGraphData: ", newGraphData);
    console.log("------------------------");
    console.log("DeleteNodeFromGraphData() - mergedGraphData: ", mergedGraphData);
    console.log("########################");
    onDeleteNode(mergedGraphData);
  }

  return (
    <button onClick={() => { deleteNode(graphData, deleteNodeId) }}>
      Delete node
    </button>
  );
}
