import { Alert, AlertIcon, Stack } from "@chakra-ui/react";
import React from "react";
import { useGraphsListContext } from "../GraphsListContext";

export const GraphWarnings = ({
  showOneShardWarning = true
}: {
  showOneShardWarning?: boolean;
}) => {
  const { isOneShardDb } = useGraphsListContext();
  return (
    <Stack>
      <Alert status="info">
        <AlertIcon />
        Only use non-existent collection names. They are automatically created
        during the graph setup.
      </Alert>
      {isOneShardDb && showOneShardWarning && (
        <Alert status="warning">
          <AlertIcon />
          Creating SmartGraphs in a OneShard database is discouraged.
          SmartGraphs only make sense when data are actually distributed, which
          is exactly opposite to the purpose of the OneShard feature. Using a
          SmartGraph in a OneShard database may lead to some OneShard query
          optimizations being disabled.
        </Alert>
      )}
    </Stack>
  );
};
