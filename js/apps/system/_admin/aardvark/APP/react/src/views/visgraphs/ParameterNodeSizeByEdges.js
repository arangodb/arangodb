import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Center } from '@chakra-ui/react';

const ParameterNodeSizeByEdges = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSizeByEdges, setNodeSizeByEdges] = useState(urlParameters.nodeSizeByEdges);

  const handleChange = (event) => {
    setNodeSizeByEdges(event.target.checked);
    const newUrlParameters = {...urlParameters, nodeSizeByEdges: event.target.checked};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row' mt='12'>
      <Center>
        <Box color='#fff' w='150px'>Size by connections</Box>
      </Center>
      <Checkbox
        inline
        checked={nodeSizeByEdges}
        onChange={handleChange}
        template={'graphviewer'}
      />
       <Center>
        <ToolTip
          title={"If enabled, nodes are sized according to the number of edges they have. This option overrides the sizing attribute."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterNodeSizeByEdges;
