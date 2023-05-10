/* global */

import React from "react";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import "./graph.css";
import { GraphDisplay } from "./GraphDisplay.tsx";

const GraphV2ReactView = () => {
  useDisableNavBar();
  useGlobalStyleReset();

  return (
    <div className="graphReactViewContainer">
      <ChakraCustomProvider>
        <GraphDisplay />
      </ChakraCustomProvider>
    </div>
  );
};

window.GraphV2ReactView = GraphV2ReactView;
