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

  const newUrlParameters = { ...urlParameters };

  return (
    <Flex direction='row' mt='12'>
      <Center>
        <Box color='#fff' w='150px'>Color nodes by collection</Box>
      </Center>
      <Checkbox
        inline
        checked={nodeColorByCollection}
        onChange={() => {
          const newNodeColorByCollection = !nodeColorByCollection;
          setNodeColorByCollection(newNodeColorByCollection);
          newUrlParameters.nodeColorByCollection = newNodeColorByCollection;
          setUrlParameters(newUrlParameters);
        }}
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
