/* global */

import React from "react";
import "./graph.css";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { ChakraProvider } from "@chakra-ui/react";
import { theme } from "../../theme/theme";
import { GraphDisplay } from "./GraphDisplay.tsx";

const VisGraphReactView = () => {
  useDisableNavBar();
  useGlobalStyleReset();

  return (
    <div className="graphReactViewContainer">
      <ChakraProvider theme={theme}>
        <GraphDisplay />
      </ChakraProvider>
    </div>
  );
};

window.VisGraphReactView = VisGraphReactView;
