/* global arangoHelper, $ */
import React, { useState } from 'react';
import styled from "styled-components";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import { AttributesInfo } from './AttributesInfo';

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

const StyledButton = styled.button`
  margin-left: 15px !important;
  color: white !important;
`;

export const DeleteNodeModal = ({ shouldShow, onDeleteNode, onRequestClose, node, nodeData, basicNodeData, editorContent, children, nodeKey, nodeCollection, graphName }) => {

  const [deleteEdges, setDeleteEdges] = useState(true);

  const deleteNode = (deleteNodeId) => {
    const slashPos = node.indexOf("/");
    const collection = node.substring(0, slashPos);
    const vertex = node.substring(slashPos + 1);
    const data = {
      "keys": [
        vertex
      ],
      "collection": collection
    }

    if(deleteEdges) {
      const url = arangoHelper.databaseUrl(
        '/_api/gharial/' + encodeURIComponent(graphName) + '/vertex/' + encodeURIComponent(collection) + '/' + encodeURIComponent(vertex)
      );

      $.ajax({
        cache: false,
        type: 'DELETE',
        contentType: 'application/json',
        url: url,
        success: function (data) {
          if(data.removed) {
            arangoHelper.arangoNotification(`The node ${deleteNodeId} and connected edges were successfully deleted`);
            onDeleteNode(deleteNodeId);
          }
        },
        error: function (e) {
          console.log("Error: ", e);
          arangoHelper.arangoError('Graph', 'Could not delete node.');
          console.log("ERROR: Could not delete edge");
        }
      });
    } else {
      $.ajax({
        cache: false,
        type: 'PUT',
        contentType: 'application/json',
        data: JSON.stringify(data),
        url: arangoHelper.databaseUrl('/_api/simple/remove-by-keys'),
        success: function (data) {
          console.log("data after deleting only the node: ", data);
          arangoHelper.arangoNotification(`The node ${deleteNodeId} was successfully deleted`);
          onDeleteNode(deleteNodeId);
        },
        error: function (e) {
          console.log("Error: ", e);
          arangoHelper.arangoError('Graph', 'Could not delete node.');
          console.log("ERROR: Could not delete edge");
        }
      });
    }

    //onDeleteNode(deleteNodeId);
  }

  return shouldShow ? (
    <ModalBackground onClick={onRequestClose}>
      <ModalBody onClick={(e) => e.stopPropagation()}>
        <div>
          {children}<br />
        </div>
        <AttributesInfo attributes={basicNodeData} />

        <Checkbox
          label='Delete connected edges too'
          inline
          checked={deleteEdges}
          onChange={() => {
            const newDeleteEdges = !deleteEdges;
            setDeleteEdges(newDeleteEdges);
          }}
        />

        <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
          <StyledButton className="button-close" onClick={onRequestClose}>Cancel</StyledButton>
          <StyledButton className="button-danger" onClick={() => { deleteNode(node) }}>Delete</StyledButton>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
