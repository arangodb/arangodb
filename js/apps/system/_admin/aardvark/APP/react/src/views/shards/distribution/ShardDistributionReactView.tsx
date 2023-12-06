import { Box, ChakraProvider } from "@chakra-ui/react";
import React from "react";
import { theme } from "../../../theme/theme";
import { RebalanceShards } from "./RebalanceShards";
import { ShardDistributionContent } from "./ShardDistributionContent";

const ShardDistributionReactView = ({ readOnly }: { readOnly: boolean }) => {
  const [refetchToken, setRefetchToken] = React.useState(() => Date.now());
  return (
    <ChakraProvider theme={theme}>
      <Box width="100%" padding="5" background="white">
        <ShardDistributionContent refetchToken={refetchToken} />
        {!readOnly && (
          <RebalanceShards refetchStats={() => setRefetchToken(Date.now())} />
        )}
      </Box>
    </ChakraProvider>
  );
};

(window as any).ShardDistributionReactView = ShardDistributionReactView;
