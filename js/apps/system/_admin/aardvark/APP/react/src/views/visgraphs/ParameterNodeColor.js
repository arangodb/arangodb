import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Textinput from "./components/pure-css/form/Textinput.tsx";
import {
  Flex,
  Center } from '@chakra-ui/react';

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const handleChange = (event) => {
    setNodeColor(event.target.value);
    const newUrlParameters = {...urlParameters, nodeColor: event.target.value.replace("#", "")};
    setUrlParameters(newUrlParameters);
  }

  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith('#')) {
    calculatedNodeColor = '#' + nodeColor;
  }

  return (
    <Flex direction='row'>
      <Center>
        <Textinput
          label={'Default node color'}
          type={'color'}
          value={calculatedNodeColor}
          template={'graphviewer'}
          width={'60px'}
          height={'30px'}
          onChange={handleChange}
          disabled={urlParameters.nodeColorByCollection}>
        </Textinput>
      </Center>
    </Flex>
  );
};

export default ParameterNodeColor;
