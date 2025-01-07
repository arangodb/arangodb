import { Box } from "@chakra-ui/react";
import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { SingleViewView } from "./editView/SingleViewView";
import { SearchViewsCustomStyleReset } from "./SearchViewsCustomStyleReset";
import { ViewsList } from "./ViewsList";

export const ViewsListWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <Box width="100%">
      <ChakraCustomProvider overrideNonReact>
        <SearchViewsCustomStyleReset>
          <HashRouter basename="/" hashType={"noslash"}>
            <Switch>
              <Route path="/views" exact>
                <ViewsList />
              </Route>
              <Route path="/view/:viewName">
                <SingleViewView />
              </Route>
            </Switch>
          </HashRouter>
        </SearchViewsCustomStyleReset>
      </ChakraCustomProvider>
    </Box>
  );
};
