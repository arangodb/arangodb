import React, { useState, useRef } from 'react';
import { JsonEditor as Editor } from 'jsoneditor-react';
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

export const AddNodeModal = ({ graphData, shouldShow, onAddNode, onRequestClose, children }) => {

  const [json, setJson] = useState({
    "id": "frenchCity/Nantes",
    "label": "Nantes",
    "style": {
      "label": {
          "value": "Nantes"
      }
    }
  });
  const jsonvalue = useRef();

  const addNode = (graphData, jsonvalue) => {

    let nodes = graphData.nodes;
    const edges = graphData.edges;
    const settings = graphData.settings;

    //const newNode = jsonvalue.current.value;
    console.log("jsonvalue: ", jsonvalue);

    // add new node
    /*
    const newNode = {
      "id": "frenchCity/Nantes",
      "label": "Nantes",
      "style": {
        "label": {
            "value": "Nantes"
        }
      }
    };
    */
    nodes.push(jsonvalue);
    console.log("nodes with new node added: ", nodes);
  
    const mergedGraphData = {
      nodes,
      edges,
      settings
    };
    //onAddNode(mergedGraphData);
  }
  //<Editor ref={jsonvalue} value={json} mode={'code'} />
  return shouldShow ? (
      <ModalBackground onClick={onRequestClose}>
        <ModalBody onClick={(e) => e.stopPropagation()}>
          <p>{children}</p>
          <textarea ref={jsonvalue}>{json.toString()}</textarea>
          <button onClick={onRequestClose}>Cancel</button><button className="button-success" onClick={() => { addNode(graphData, jsonvalue.current.value) }}>Add node</button>
        </ModalBody>
      </ModalBackground>
  ) : null;
};
