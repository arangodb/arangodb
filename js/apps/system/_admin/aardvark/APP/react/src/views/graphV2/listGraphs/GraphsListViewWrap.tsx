import { Box } from "@chakra-ui/react";
import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { GraphsListView } from "./GraphsListView";

export const GraphsListViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <Box width="100%">
      <ChakraCustomProvider overrideNonReact>
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/graphs" exact>
              <GraphsListView />
            </Route>
          </Switch>
        </HashRouter>
      </ChakraCustomProvider>
    </Box>
  );
};
