import { Button, Stack } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";

export const QueryEditorTopBar = () => {
  const {
    setCurrentView,
    currentView,
    onQueryChange,
    queryName,
    queryValue,
    queryBindParams,
    resetEditor,
    setResetEditor
  } = useQueryContext();
  const showNewButton =
    queryName !== "" ||
    queryValue !== "" ||
    JSON.stringify(queryBindParams) !== "{}";
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
      {showNewButton && (
        <Button
          size="sm"
          colorScheme="gray"
          onClick={() => {
            onQueryChange({
              value: "",
              parameter: {},
              name: ""
            });
            setResetEditor(!resetEditor);
          }}
        >
          New
        </Button>
      )}
    </Stack>
  );
};
