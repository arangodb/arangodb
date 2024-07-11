import { Button, Flex, Stack, Text } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { SaveAsModal } from "./SaveAsModal";

export const QueryEditorBottomBar = () => {
  const {
    onExecute,
    onProfile,
    onExplain,
    queryValue,
    queryBindParams,
    currentQueryName,
    onOpenSaveAsModal,
    onSave,
    savedQueries,
    queryOptions,
    disabledRules
  } = useQueryContext();
  const existingQuery = currentQueryName
    ? savedQueries?.find(query => query.name === currentQueryName)
    : null;
  const hasQueryChanged =
    existingQuery?.value !== queryValue ||
    JSON.stringify(existingQuery?.parameter) !==
      JSON.stringify(queryBindParams);
  const queryName =
    existingQuery && currentQueryName ? currentQueryName : "Untitled";
  return (
    <Flex
      direction="row"
      borderX="1px solid"
      borderColor="gray.300"
      padding="2"
      alignItems="center"
    >
      <SaveAsModal />

      <Text fontWeight="medium">
        Query name:
        <Text isTruncated title={queryName} maxWidth="500px">
          {queryName}
        </Text>
      </Text>
      {existingQuery && currentQueryName && (
        <Button
          marginLeft="2"
          size="sm"
          colorScheme="gray"
          isDisabled={!hasQueryChanged}
          onClick={() => onSave(currentQueryName)}
        >
          Save
        </Button>
      )}
      <Button
        marginLeft="2"
        size="sm"
        colorScheme="gray"
        onClick={onOpenSaveAsModal}
      >
        Save as
      </Button>
      <Stack direction={"row"} marginLeft="auto">
        <Button
          size="sm"
          colorScheme="gray"
          variant={"ghost"}
          onClick={() =>
            onExplain({
              queryValue,
              queryBindParams,
              queryOptions,
              disabledRules
            })
          }
        >
          Explain
        </Button>
        <Button
          size="sm"
          colorScheme="gray"
          onClick={() =>
            onProfile({
              queryValue,
              queryBindParams,
              queryOptions,
              disabledRules
            })
          }
        >
          Profile
        </Button>
        <Button
          size="sm"
          colorScheme="green"
          onClick={() =>
            onExecute({
              queryValue,
              queryBindParams,
              queryOptions,
              disabledRules
            })
          }
        >
          Execute
        </Button>
      </Stack>
    </Flex>
  );
};
