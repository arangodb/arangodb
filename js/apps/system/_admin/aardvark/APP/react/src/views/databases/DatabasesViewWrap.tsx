import { Box } from "@chakra-ui/react";
import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { DatabasesProvider } from "./DatabasesContext";
import { DatabasesView } from "./DatabasesView";
import { SingleDatabaseView } from "./viewDatabase/SingleDatabaseView";

export const DatabasesViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <Box width="100%">
      <ChakraCustomProvider overrideNonReact>
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/databases" exact>
              <DatabasesProvider>
                <DatabasesView />
              </DatabasesProvider>
            </Route>
            <Route path="/databases/:databaseName">
              <DatabasesProvider isFormDisabled>
                <SingleDatabaseView />
              </DatabasesProvider>
            </Route>
          </Switch>
        </HashRouter>
      </ChakraCustomProvider>
    </Box>
  );
};
