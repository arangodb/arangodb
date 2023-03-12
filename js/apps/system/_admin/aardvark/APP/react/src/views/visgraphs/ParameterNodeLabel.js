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

  const newUrlParameters = { ...urlParameters };
  
  return (
    <Flex direction='row' mt='24' mb='12'>
      <Center>
        <Box color='#fff' w='150px'>Node label</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={nodeLabel}
        onChange={(e) => {
          setNodeLabel(e.target.value);
          newUrlParameters.nodeLabel = e.target.value;
          setUrlParameters(newUrlParameters);
        }}
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
