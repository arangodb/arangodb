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

export const DeleteEdgeModal = ({ shouldShow, onDeleteEdge, onRequestClose, edge, edgeData, basicEdgeData, editorContent, children, edgeKey, edgeCollection }) => {

  const openNotificationWithIcon = type => {
    notification[type]({
      message: 'Edge deletes',
      description:
        `The edge ${edge} was successfully deleted`,
    });
  };

  const deleteEdge = (deleteEdgeId) => {
    const slashPos = edge.indexOf("/");
    const data = {
      "keys": [
        edge.substring(slashPos + 1)
      ],
      "collection": edge.substring(0, slashPos)
    }

    $.ajax({
      cache: false,
      type: 'PUT',
      contentType: 'application/json',
      data: JSON.stringify(data),
      url: arangoHelper.databaseUrl('/_api/simple/remove-by-keys'),
      success: function () {
        openNotificationWithIcon('success');
      },
      error: function () {
        console.log("ERROR: Could not delete edge");
      }
    });
    onDeleteEdge(deleteEdgeId);
  }

  return shouldShow ? (
    <ModalBackground onClick={onRequestClose}>
      <ModalBody onClick={(e) => e.stopPropagation()}>
        <div>
          {children}<br />
        </div>
        <AttributesInfo attributes={basicEdgeData} />
        <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
          <button className="button-close" onClick={onRequestClose}>Cancel</button>
          <button className="button-danger" onClick={() => { deleteEdge(edge) }}>Delete</button>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
