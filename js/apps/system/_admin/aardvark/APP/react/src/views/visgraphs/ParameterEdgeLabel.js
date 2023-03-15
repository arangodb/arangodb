import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterEdgeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabel, setEdgeLabel] = useState(urlParameters.edgeLabel);

  const handleChange = (event) => {
    setEdgeLabel(event.target.value);
    const newUrlParameters = {...urlParameters, edgeLabel: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row' mt='24' mb='12'>
      <Center>
        <Box color='#fff' w='150px'>Edge label</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={edgeLabel}
        onChange={handleChange}
      />
      <Spacer />
      <Center>
        <ToolTip
          title={"Enter a valid edge attribute to be used as an edge label."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterEdgeLabel;
