/* global arangoHelper, $ */
import React, { useState, useRef } from 'react';
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
  margin: 5% auto;
  padding: 20px;
  width: 50%;
`;

const StyledButton = styled.button`
  margin-left: 15px !important;
  color: white !important;
`;

export const HelpModal = ({ shouldShow, onRequestClose, children }) => {

  return shouldShow ? (
    <ModalBackground onClick={onRequestClose}>
      <ModalBody onClick={(e) => e.stopPropagation()}>
        <div>
          <span class="arangoHeader">Instructions</span>
          <hr />
          <dl>
            <dt>Creating an edge</dt>
            <dd>
              Press and hold shift, then click the two nodes you want to connect with an edge. The first node will be the <code>_from</code> node and the second one the <code>_to</code> node. A modal opens to:
              <ul>
                <li>Set a "_key" (optional) </li>
                <li>Select the edge collection you want the edge to be saved in</li>
              </ul>
            </dd>
          </dl>
        </div>
        <div style={{ 'marginTop': '38px', 'textAlign': 'right' }}>
          <StyledButton className="button-success" onClick={onRequestClose}>Close</StyledButton>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
  
};
