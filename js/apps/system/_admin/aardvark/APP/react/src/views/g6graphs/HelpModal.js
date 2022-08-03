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
          {children}<br />
          <strong>content from the modal</strong>
        </div>
        <div style={{ 'marginTop': '38px', 'textAlign': 'right' }}>
          <StyledButton className="button-success" onClick={onRequestClose}>Close</StyledButton>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
  
};
