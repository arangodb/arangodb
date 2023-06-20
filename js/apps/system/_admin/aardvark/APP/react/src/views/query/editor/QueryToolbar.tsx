import { Button, Stack } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";

export const QueryToolbar = () => {
  const { setCurrentView, currentView } = useQueryContext();
  return (
    <Stack
      direction="row"
      padding="2"
      borderBottom="1px solid"
      borderBottomColor="gray.300"
    >
      <Button
        size="sm"
        colorScheme="gray"
        onClick={() => {
          setCurrentView(currentView === "saved" ? "editor" : "saved");
        }}
      >
        Saved Queries
      </Button>
      <Button size="sm" colorScheme="gray">
        New
      </Button>
      <Button size="sm" colorScheme="gray">
        Save as
      </Button>
    </Stack>
  );
};
