import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Center } from '@chakra-ui/react';

const ParameterEdgeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabelByCollection, setEdgeLabelByCollection] = useState(urlParameters.edgeLabelByCollection);

  const handleChange = (event) => {
    setEdgeLabelByCollection(event.target.checked);
    const newUrlParameters = {...urlParameters, edgeLabelByCollection: event.target.checked};
    setUrlParameters(newUrlParameters);
  }
  
  return (
    <Flex direction='row' mt='12'>
      <Center>
        <Box color='#fff' w='150px'>Show collection name</Box>
      </Center>
      <Checkbox
        inline
        checked={edgeLabelByCollection}
        onChange={handleChange}
        template={'graphviewer'}
      />
      <Center>
        <ToolTip
          title={"Adds a collection name to the edge label."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterEdgeLabelByCollection;
