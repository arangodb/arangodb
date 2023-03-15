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

  const getNewUrlParams = (urlParams, newEdgeDirection) => {
    setEdgeDirection(newEdgeDirection);
    const newUrlParameters = {...urlParameters, edgeDirection: newEdgeDirection};
    setUrlParameters(newUrlParameters);
  }

  return (
    <Flex direction='row' mt='12'>
      <Center>
        <Box color='#fff' w='150px'>Show edge direction</Box>
      </Center>
      <Checkbox
        inline
        checked={edgeDirection}
        onChange={(event) => {
          getNewUrlParams(urlParameters, event.target.checked);
        }}
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
