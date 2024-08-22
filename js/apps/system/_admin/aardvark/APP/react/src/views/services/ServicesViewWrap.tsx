import { Box } from "@chakra-ui/react";
import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { ServicesProvider } from "./ServicesContext";
import { ServicesView } from "./ServicesView";

export const ServicesViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <Box width="100%">
      <ChakraCustomProvider overrideNonReact>
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/services" exact>
              <ServicesProvider>
                <ServicesView />
              </ServicesProvider>
            </Route>
          </Switch>
        </HashRouter>
      </ChakraCustomProvider>
    </Box>
  );
};
