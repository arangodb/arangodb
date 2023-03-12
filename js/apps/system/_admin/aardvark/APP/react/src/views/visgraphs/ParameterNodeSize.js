import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const newUrlParameters = { ...urlParameters };

  return (
    <Flex direction='row' mt='24' mb='12'>
      <Center>
        <Box color='#fff' w='150px'>Sizing attribute</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={nodeSize}
        onChange={(e) => {
          setNodeSize(e.target.value);
          newUrlParameters.nodeSize = e.target.value;
          setUrlParameters(newUrlParameters);
        }}
        disabled={urlParameters.nodeSizeByEdges}
      />
      <Spacer />
      <Center>
        <ToolTip
          title={"If an attribute is given, nodes will be sized by the attribute."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterNodeSize;
