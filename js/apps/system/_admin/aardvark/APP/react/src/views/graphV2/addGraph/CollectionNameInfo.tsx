import { Alert, AlertIcon } from "@chakra-ui/react";
import React from "react";

export const CollectionNameInfo = () => {
  return (
    <Alert status="info">
      <AlertIcon />
      Only use non-existent collection names. They are automatically created
      during the graph setup.
    </Alert>
  );
};
