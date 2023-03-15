import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterDepth = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [depth, setDepth] = useState(urlParameters.depth);

  const handleChange = (event) => {
    setDepth(event.target.value);
    const newUrlParameters = {...urlParameters, depth: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row'>
      <Center>
        <Box color='#fff' w='150px'>Depth</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={depth}
        onChange={handleChange}
      />
      <Spacer />
      <Center>
        <ToolTip
          title={"Search depth, starting from your start node."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterDepth;
