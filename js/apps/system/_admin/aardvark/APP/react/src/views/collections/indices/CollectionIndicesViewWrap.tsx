import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { CollectionIndicesProvider } from "./CollectionIndicesContext";
import { CollectionIndicesView } from "./CollectionIndicesView";

export const CollectionIndicesViewWrap = ({
  collectionName,
  collectionId
}: {
  collectionName: string;
  collectionId: string;
}) => {
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider overrideNonReact>
      <CollectionIndicesProvider
        collectionName={collectionName}
        collectionId={collectionId}
      >
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/cIndices/:collectionName">
              <CollectionIndicesView />
            </Route>
          </Switch>
        </HashRouter>
      </CollectionIndicesProvider>
    </ChakraCustomProvider>
  );
};
