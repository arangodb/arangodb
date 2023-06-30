import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { GraphsView } from "./GraphsView";

export const GraphsViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider overrideNonReact>
      <HashRouter basename="/" hashType={"noslash"}>
        <Switch>
          <Route path="/graphs" exact>
            <GraphsView />
          </Route>
        </Switch>
      </HashRouter>
    </ChakraCustomProvider>
  );
};
