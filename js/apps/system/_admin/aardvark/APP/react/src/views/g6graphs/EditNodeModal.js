/* global arangoHelper, $ */
import React, { useState, useRef } from 'react';
import styled from "styled-components";
import { JsonEditor as Editor } from 'jsoneditor-react';
import {  notification } from 'antd';
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

  & div, & button {
    box-sizing: content-box !important;
  }
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

export const EditNodeModal = ({ shouldShow, onUpdateNode, onRequestClose, node, nodeData, basicNodeData, editorContent, children, nodeKey, nodeCollection }) => {

  const jsonEditorRef = useRef();
  const [json, setJson] = useState(nodeData);

  const openNotificationWithIcon = type => {
    arangoHelper.arangoNotification(`The node ${node} was successfully updated`);
  };

  const updateNode = (graphData, updateNodeId) => {
    const newNodeData = {
        ...basicNodeData,
        ...json
    };
    $.ajax({
      cache: false,
      type: 'PUT',
      url: arangoHelper.databaseUrl('/_api/document/' + nodeCollection + '?returnNew=true'),
      data: JSON.stringify([newNodeData]),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        openNotificationWithIcon('success');
      },
      error: function (data) {
        console.log("Error saving document: ", data);
        arangoHelper.arangoError('Graph', 'Could not update this node.');
      }
    });
    onRequestClose();
  }

  return shouldShow ? (
    <ModalBackground onClick={onRequestClose}>
      <ModalBody onClick={(e) => e.stopPropagation()}>
        <div>
          {children}<br />
        </div>
        <AttributesInfo attributes={basicNodeData} />
        <div>
          {
            nodeData ? (
              <Editor
                ref={jsonEditorRef}
                value={nodeData}
                onChange={(value) => {
                  setJson(value);
                }}
                mode={'code'}
                history={true} />
            ) : 'Loading data...'
          }
        </div>
        <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
          <StyledButton className="button-close" onClick={onRequestClose}>Cancel</StyledButton>
          <StyledButton className="button-success" onClick={() => { updateNode(node) }}>Update</StyledButton>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
