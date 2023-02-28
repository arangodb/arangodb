/* global frontendConfig */
import { ChakraProvider } from "@chakra-ui/react";
import React from "react";
import { ViewSettings } from "./ViewSettings";

const ViewSettingsReactView = ({ name }) => {
  return (
    <ChakraProvider>
      <ViewSettings isCluster={frontendConfig.isCluster} name={name} />
    </ChakraProvider>
  );
};
window.ViewSettingsReactView = ViewSettingsReactView;
