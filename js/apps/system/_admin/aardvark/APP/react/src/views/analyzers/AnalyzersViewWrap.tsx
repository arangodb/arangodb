import React from "react";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { AnalyzersView } from "./AnalyzersView";

export const AnalyzersViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider>
      <AnalyzersView />
    </ChakraCustomProvider>
  );
};
