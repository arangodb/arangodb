/* global arangoHelper, $ */
import React, { useState, useRef } from 'react';
import styled from "styled-components";
import { JsonEditor as Editor } from 'jsoneditor-react';
import { notification } from 'antd';
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

export const EditEdgeModal = ({ shouldShow, onUpdateEdge, onRequestClose, edge, edgeData, basicEdgeData, editorContent, children, edgeKey, edgeCollection }) => {

  const jsonEditorRef = useRef();
  const [json, setJson] = useState(edgeData);

  const openNotificationWithIcon = type => {
    notification[type]({
      message: 'Edge updates',
      description:
        `The edge ${edge} was successfully updated`,
    });
  };

  const updateEdge = (graphData, updateEdgeId) => {
    const newEdgeData = {
      ...basicEdgeData,
      ...json
    };
    
    $.ajax({
      cache: false,
      type: 'PUT',
      url: arangoHelper.databaseUrl('/_api/document/' + edgeCollection + '?returnNew=true'),
      data: JSON.stringify([newEdgeData]),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        openNotificationWithIcon('success');
      },
      error: function (data) {
        console.log("Error saving document: ", data);
        arangoHelper.arangoError('Graph', 'Could not update this edge.');
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
        <AttributesInfo attributes={basicEdgeData} />
        <div>
          {
            edgeData ? (
              <Editor
                ref={jsonEditorRef}
                value={edgeData}
                onChange={(value) => {
                  setJson(value);
                }}
                mode={'code'}
                history={true} />
            ) : 'Data is loading...'
          }
        </div>
        <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
          <button className="button-close" onClick={onRequestClose}>Cancel</button>
          <button className="button-success" onClick={() => { updateEdge(edge) }}>Update</button>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
