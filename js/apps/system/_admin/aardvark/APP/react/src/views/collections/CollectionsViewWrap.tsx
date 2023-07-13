import React from "react";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { CollectionsProvider } from "./CollectionsContext";
import { CollectionsView } from "./CollectionsView";

export const CollectionsViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider overrideNonReact>
      <CollectionsProvider>
        <CollectionsView />
      </CollectionsProvider>
    </ChakraCustomProvider>
  );
};
