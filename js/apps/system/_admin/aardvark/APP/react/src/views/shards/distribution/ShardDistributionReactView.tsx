import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { ShardDistributionContent } from "./ShardDistributionContent";

const ShardDistributionReactView = ({ readOnly }: { readOnly: boolean }) => {
  return (
    <ChakraCustomProvider>
      <ShardDistributionContent readOnly={readOnly} />
    </ChakraCustomProvider>
  );
};

window.ShardDistributionReactView = ShardDistributionReactView;
