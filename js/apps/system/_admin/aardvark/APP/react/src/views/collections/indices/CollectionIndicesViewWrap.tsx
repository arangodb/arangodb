import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { CollectionIndicesProvider } from "./CollectionIndicesContext";
import { CollectionIndicesView } from "./CollectionIndicesView";
import { CollectionIndexDetails } from "./viewIndex/CollectionIndexDetails";

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
            <Route path="/cIndices/:collectionName" exact>
              <CollectionIndicesView />
            </Route>
            <Route path="/cIndices/:collectionName/:indexId">
              <CollectionIndexDetails />
            </Route>
          </Switch>
        </HashRouter>
      </CollectionIndicesProvider>
    </ChakraCustomProvider>
  );
};
