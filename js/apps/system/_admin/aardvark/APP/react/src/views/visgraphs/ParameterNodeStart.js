import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterNodeStart = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeStart, setNodeStart] = useState(urlParameters.nodeStart);

  const handleChange = (event) => {
    setNodeStart(event.target.value);
    const newUrlParameters = {...urlParameters, nodeStart: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row' mt='3'>
      <Center>
        <Box color='#fff' w='150px'>Start node</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={urlParameters.nodeStart}
        background='#ffffff'
        size='sm'
        onChange={handleChange}
      />
      <Spacer />
      <Center>
        <ToolTip
          title={"A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterNodeStart;
