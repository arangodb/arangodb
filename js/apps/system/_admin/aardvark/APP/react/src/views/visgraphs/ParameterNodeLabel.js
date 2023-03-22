import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterNodeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabel, setNodeLabel] = useState(urlParameters.nodeLabel);

  const handleChange = (event) => {
    setNodeLabel(event.target.value);
    const newUrlParameters = {...urlParameters, nodeLabel: event.target.value};
    setUrlParameters(newUrlParameters);
  }
  
  return (
    <Flex direction='row'>
      <Center>
        <Box color='#fff' w='150px'>Node label</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={nodeLabel}
        background='#ffffff'
        size='sm'
        onChange={handleChange}
      />
      <Spacer />
      <Center>
        <ToolTip
          title={"Enter a valid node attribute to be used as a node label."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterNodeLabel;
