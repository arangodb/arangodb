import { Button, Flex, Icon, IconButton } from "@chakra-ui/react";
import React from "react";
import { MagicWand } from "styled-icons/boxicons-solid";
import { useQueryContext } from "../QueryContextProvider";
import { QuerySpotlight } from "./QuerySpotlight";

export const QueryEditorTopBar = () => {
  const {
    setCurrentView,
    currentView,
    onQueryChange,
    queryName,
    queryValue,
    queryBindParams,
    resetEditor,
    setResetEditor,
    setIsSpotlightOpen
  } = useQueryContext();
  const showNewButton =
    queryName !== "" ||
    queryValue !== "" ||
    JSON.stringify(queryBindParams) !== "{}";
  return (
    <Flex
      gap="2"
      direction="row"
      paddingY="2"
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
      <Flex marginLeft="auto">
        <IconButton
          onClick={() => {
            setIsSpotlightOpen(true);
          }}
          icon={<Icon as={MagicWand} />}
          aria-label={"Spotlight"}
        />
      </Flex>
      <QuerySpotlight />
    </Flex>
  );
};
