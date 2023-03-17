import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Textinput from "./components/pure-css/form/Textinput.tsx";
import {
  Flex,
  Center } from '@chakra-ui/react';

const ParameterEdgeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColor, setEdgeColor] = useState(urlParameters.edgeColor);

  const handleChange = (event) => {
    setEdgeColor(event.target.value);
    const newUrlParameters = {...urlParameters, edgeColor: event.target.value.replace("#", "")};
    setUrlParameters(newUrlParameters);
  }
  let calculatedEdgeColor = edgeColor;
  if (!edgeColor.startsWith('#')) {
    calculatedEdgeColor = '#' + edgeColor;
  }

  return (
    <Flex direction='row'>
      <Center>
      <Textinput
        label={'Default edge color'}
        type={'color'}
        value={calculatedEdgeColor}
        template={'graphviewer'}
        width={'60px'}
        height={'30px'}
        onChange={handleChange}
      disabled={urlParameters.edgeColorByCollection}>
      </Textinput>
      </Center>
    </Flex>
  );
};

export default ParameterEdgeColor;
