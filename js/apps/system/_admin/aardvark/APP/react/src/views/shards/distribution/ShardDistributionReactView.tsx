import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { ShardDistributionContent } from "./ShardDistributionContent";

const ShardDistributionReactView = ({ readOnly }: { readOnly: boolean }) => {
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider>
      <ShardDistributionContent readOnly={readOnly} />
    </ChakraCustomProvider>
  );
};

window.ShardDistributionReactView = ShardDistributionReactView;
