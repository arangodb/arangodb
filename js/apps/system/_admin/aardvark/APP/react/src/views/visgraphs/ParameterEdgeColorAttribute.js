import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterEdgeColorAttribute = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorAttribute, setEdgeColorAttribute] = useState(urlParameters.edgeColorAttribute);

  const handleChange = (event) => {
    setEdgeColorAttribute(event.target.value);
    const newUrlParameters = {...urlParameters, edgeColorAttribute: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row' mt='24' mb='12'>
      <Center>
        <Box color='#fff' w='150px'>Edge color attribute</Box>
      </Center>
      <Input  
        width='200px'
        min={1}
        value={edgeColorAttribute}
        onChange={handleChange}
        disabled={urlParameters.edgeColorByCollection}
      />
      <Spacer />
      <Center>
        <ToolTip
          title={"If an attribute is given, edges will be colorized by the attribute. This setting ignores default edge color if set."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterEdgeColorAttribute;
