/* global arangoHelper, arangoFetch, frontendConfig, document, $ */
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
`;

const ModalBody = styled.div`
  background-color: white;
  margin: 10% auto;
  padding: 20px;
  width: 50%;
`;

  export const EditModal = ({ shouldShow, onUpdateNode, onRequestClose, node, nodeData, basicNodeData, editorContent, children, nodeKey, nodeCollection }) => {

    const jsonEditorRef = useRef();
    const [json, setJson] = useState(nodeData);

    const openNotificationWithIcon = type => {
      notification[type]({
        message: 'Node updates',
        description:
          `The node ${node} was successfully updated`,
      });
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
              <button className="button-close" onClick={onRequestClose}>Cancel</button>
              <button className="button-success" onClick={() => { updateNode(node) }}>Update</button>
            </div>
          </ModalBody>
        </ModalBackground>
    ) : null;
};
