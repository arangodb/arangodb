import { Box } from "@chakra-ui/react";
import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { AnalyzersProvider } from "./AnalyzersContext";
import { AnalyzersView } from "./AnalyzersView";
import { SingleAnalyzerView } from "./viewAnalyzer/SingleAnalyzerView";

export const AnalyzersViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <Box width="100%">
      <ChakraCustomProvider overrideNonReact>
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/analyzers" exact>
              <AnalyzersProvider>
                <AnalyzersView />
              </AnalyzersProvider>
            </Route>
            <Route path="/analyzers/:analyzerName">
              <AnalyzersProvider isFormDisabled>
                <SingleAnalyzerView />
              </AnalyzersProvider>
            </Route>
          </Switch>
        </HashRouter>
      </ChakraCustomProvider>
    </Box>
  );
};
