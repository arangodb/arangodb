import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { SingleViewView } from "./editView/SingleViewView";
import { SearchViewsCustomStyleReset } from "./SearchViewsCustomStyleReset";
import { ViewsProvider } from "./ViewsContext";
import { ViewsView } from "./ViewsView";

export const ViewsViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider overrideNonReact>
      <SearchViewsCustomStyleReset>
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/views" exact>
              <ViewsProvider>
                <ViewsView />
              </ViewsProvider>
            </Route>
            <Route path="/views/:viewName">
              <ViewsProvider isFormDisabled>
                <SingleViewView />
              </ViewsProvider>
            </Route>
          </Switch>
        </HashRouter>
      </SearchViewsCustomStyleReset>
    </ChakraCustomProvider>
  );
};
