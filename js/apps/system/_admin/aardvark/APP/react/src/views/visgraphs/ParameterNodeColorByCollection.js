import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Center } from '@chakra-ui/react';

const ParameterNodeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorByCollection, setNodeColorByCollection] = useState(urlParameters.nodeColorByCollection);

  const handleChange = (event) => {
    setNodeColorByCollection(event.target.checked);
    const newUrlParameters = {...urlParameters, nodeColorByCollection: event.target.checked};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row'>
      <Center>
        <Box color='#fff' w='150px'>Color nodes by collection</Box>
      </Center>
      <Checkbox
        inline
        checked={nodeColorByCollection}
        onChange={handleChange}
        template={'graphviewer'}
      />
       <Center>
        <ToolTip
          title={"Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>

    </Flex>
  );
};

export default ParameterNodeColorByCollection;
