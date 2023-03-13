/* global */

import React from 'react';
import './graph.css';
import VisJsGraph from './VisJsGraph';
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { ChakraProvider } from "@chakra-ui/react";
import { theme } from "../../theme/theme";

const VisGraphReactView = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  
  return <div className="graphReactViewContainer">
    <ChakraProvider theme={theme}>
      <VisJsGraph />
    </ChakraProvider>
  </div>;
};

window.VisGraphReactView = VisGraphReactView;
