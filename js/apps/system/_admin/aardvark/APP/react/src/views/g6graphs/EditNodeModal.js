import styled from "styled-components";


const ModalBackground = styled.div`
  position: fixed;
  z-index: 1;
  left: 0;
  top: 0;
  width: 100%;
  height: 100%;
  overflow: auto;
  background-color: rgba(0, 0, 0, 0.5);
`;

const ModalBody = styled.div`
  background-color: white;
  margin: 10% auto;
  padding: 20px;
  width: 50%;
`;

export const EditNodeModal = ({ graphData, shouldShow, onUpdateNode, onRequestClose, node, children }) => {
  const updateNode = (graphData, updateNodeId) => {
    console.log("#################");
    console.log("EditNodeFromGraphData in updateNode (node): ", node);
    console.log("#################");
    console.log("EditNodeFromGraphData in updateNode (graphData): ", graphData);
    console.log("EditNodeFromGraphData in updateNode (updateNodeId): ", updateNodeId);

    console.log("EditNodeFromGraphData() - graphData.nodes: ", graphData.nodes);
    console.log("EditNodeFromGraphData() - graphData.edges: ", graphData.edges);
    console.log("EditNodeFromGraphData() - graphData.settings: ", graphData.settings);

    // delete node from nodes
    const nodes = graphData.nodes.filter((node) => {
      return node.id !== updateNodeId;
    });

    // delete edges with updateNodeId as source
    let edges = graphData.edges.filter((edge) => {
      return edge.source !== updateNodeId;
    });

    // delete edges with updateNodeId as target
    edges = edges.filter((edge) => {
      return edge.target !== updateNodeId;
    });
   
    //const edges = graphData.edges;
    const settings = graphData.settings;
    const mergedGraphData = {
      nodes,
      edges,
      settings
    };
    console.log("########################");
    console.log("EditNodeFromGraphData() - nodes: ", nodes);
    console.log("EditNodeFromGraphData() - edges: ", edges);
    //console.log("EditNodeFromGraphData() - newGraphData: ", newGraphData);
    console.log("------------------------");
    console.log("EditNodeFromGraphData() - mergedGraphData: ", mergedGraphData);
    console.log("########################");
    onUpdateNode(mergedGraphData);
  }
  return shouldShow ? (
      <ModalBackground onClick={onRequestClose}>
        <ModalBody onClick={(e) => e.stopPropagation()}>
          <p>{children}<br />
          nodeId: {node.id}</p>
          <button onClick={onRequestClose}>Cancel</button><button className="button-success" onClick={() => { updateNode(graphData, node.id) }}>Update</button>
        </ModalBody>
      </ModalBackground>
  ) : null;
};
