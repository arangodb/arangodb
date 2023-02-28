/* global frontendConfig */

import { ChakraProvider } from "@chakra-ui/react";
import React from "react";
import "./split-pane-styles.css";
import { ViewSettings } from "./ViewSettings";
import "./viewsheader.css";

const ViewSettingsReactView = ({ name }) => {
  return (
    <ChakraProvider>
      <ViewSettings
        isCluster={frontendConfig.isCluster}
        name={name}
      />
    </ChakraProvider>
  );
};
window.ViewSettingsReactView = ViewSettingsReactView;
