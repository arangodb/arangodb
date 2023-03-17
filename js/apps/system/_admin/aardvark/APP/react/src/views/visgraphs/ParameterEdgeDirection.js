import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Center } from '@chakra-ui/react';

const ParameterEdgeDirection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeDirection, setEdgeDirection] = useState(urlParameters.edgeDirection);

  const handleChange = (event) => {
    setEdgeDirection(event.target.checked);
    const newUrlParameters = {...urlParameters, edgeDirection: event.target.checked};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row'>
      <Center>
        <Box color='#fff' w='150px'>Show edge direction</Box>
      </Center>
      <Checkbox
        inline
        checked={edgeDirection}
        onChange={handleChange}
        template={'graphviewer'}
      />
      <Center>
        <ToolTip
          title={"When true, an arrowhead on the 'to' side of the edge is drawn."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>
    </Flex>
  );
};

export default ParameterEdgeDirection;
