import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';


const ParameterNodeColorAttribute = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorAttribute, setNodeColorAttribute] = useState(urlParameters.nodeColorAttribute);

  const handleChange = (event) => {
    setNodeColorAttribute(event.target.value);
    const newUrlParameters = {...urlParameters, nodeColorAttribute: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row' mt='24' mb='12'>
      <Center>
        <Box color='#fff' w='150px'>Node color attribute</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        required={true}
        value={nodeColorAttribute}
        onChange={handleChange}
        disabled={urlParameters.nodeColorByCollection}
      />
      <Spacer />
      <Center>
        <ToolTip
            title={"If an attribute is given, nodes will be colorized by the attribute. This setting ignores default node color if set."}
            setArrow={true}
          >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterNodeColorAttribute;
