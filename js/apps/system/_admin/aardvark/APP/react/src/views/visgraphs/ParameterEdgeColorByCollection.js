import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Center } from '@chakra-ui/react';

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(urlParameters.edgeColorByCollection);

  const handleChange = (event) => {
    setEdgeColorByCollection(event.target.checked);
    const newUrlParameters = {...urlParameters, edgeColorByCollection: event.target.checked};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row'>
      <Center>
        <Box color='#fff' w='150px'>Color edges by collection</Box>
      </Center>
      <Checkbox
        inline
        checked={edgeColorByCollection}
        onChange={handleChange}
        template={'graphviewer'}
      />
      <Center>
        <ToolTip
          title={"Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterEdgeColorByCollection;
