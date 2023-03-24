/* global arangoHelper, $ */
import React, { useContext } from 'react';
import styled from "styled-components";
import { UrlParametersContext } from "./url-parameters-context";
import {
  Flex,
  Spacer } from '@chakra-ui/react';

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

export const FetchFullGraphModal = ({ shouldShow, onRequestClose, children, onFullGraphLoaded, graphName }) => {
let responseTimesObject = {
    fetchStarted: null,
    fetchFinished: null,
    fetchDuration: null
  }
  const urlParameters = useContext(UrlParametersContext);

  const fetchFullGraph = () => {
    let newResponseTimesObject = {...responseTimesObject, fetchStarted: new Date()};
    const newUrlParameters = {...urlParameters[0], mode: "all"};

    $.ajax({
      type: 'GET',
      url: arangoHelper.databaseUrl(`/_admin/aardvark/visgraph/${graphName}`),
      contentType: 'application/json',
      data: newUrlParameters,
      success: function (data) {
        newResponseTimesObject = {...newResponseTimesObject, fetchFinished: new Date()};

        const fetchDuration = Math.abs(newResponseTimesObject.fetchFinished.getTime() - newResponseTimesObject.fetchStarted.getTime());
        newResponseTimesObject = {...newResponseTimesObject, fetchDuration: fetchDuration};
        onRequestClose();
        onFullGraphLoaded(data, newResponseTimesObject);
      },
      error: function (e) {
        console.log(e);
        arangoHelper.arangoError('Graph', e.responseJSON.errorMessage);
      }
    });
  }

  return shouldShow ? (
    <ModalBackground onClick={onRequestClose}>
      <ModalBody onClick={(e) => e.stopPropagation()}>
        <div>
          {children}<br />
        </div>
        <Flex direction='row' mt='38'>
          <Spacer />
          <StyledButton className="button-close" onClick={onRequestClose}>Cancel</StyledButton>
          <StyledButton className="button-success" onClick={() => { fetchFullGraph() }}>Load full graph</StyledButton>
        </Flex>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
