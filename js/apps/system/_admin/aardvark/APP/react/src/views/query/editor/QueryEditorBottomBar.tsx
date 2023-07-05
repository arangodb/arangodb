import { ChevronDownIcon } from "@chakra-ui/icons";
import {
  Button,
  ButtonGroup,
  Flex,
  IconButton,
  Menu,
  MenuButton,
  MenuItem,
  MenuList,
  Stack,
  Text,
  useDisclosure
} from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { DebugPackageModal } from "./DebugPackageModal";
import { SaveAsModal } from "./SaveAsModal";

export const QueryEditorBottomBar = () => {
  const {
    onExecute,
    onProfile,
    onExplain,
    queryValue,
    queryBindParams,
    queryResults,
    setQueryResults,
    queryName,
    onOpenSaveAsModal,
    onSave,
    savedQueries
  } = useQueryContext();
  const existingQuery = queryName
    ? savedQueries?.find(query => query.name === queryName)
    : null;
  const hasQueryChanged =
    existingQuery?.value !== queryValue ||
    JSON.stringify(existingQuery?.parameter) !==
      JSON.stringify(queryBindParams);
  const {
    isOpen: isDebugPackageModalOpen,
    onOpen: onOpenDebugPackageModal,
    onClose: onCloseDebugPackageModal
  } = useDisclosure();
  return (
    <Flex
      direction="row"
      borderBottom="1px solid"
      borderX="1px solid"
      borderColor="gray.300"
      padding="2"
      alignItems="center"
    >
      <SaveAsModal />
      <DebugPackageModal
        isDebugPackageModalOpen={isDebugPackageModalOpen}
        onCloseDebugPackageModal={onCloseDebugPackageModal}
      />
      <Text fontWeight="medium">
        Query name: {queryName ? queryName : "Untitled"}
      </Text>
      {queryName && (
        <Button
          marginLeft="2"
          size="sm"
          colorScheme="gray"
          isDisabled={!hasQueryChanged}
          onClick={() => onSave(queryName)}
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
        {queryResults.length > 0 ? (
          <Button
            size="sm"
            colorScheme="gray"
            onClick={() => {
              setQueryResults([]);
            }}
          >
            Remove all results
          </Button>
        ) : null}
        <Button size="sm" colorScheme="gray" onClick={onOpenDebugPackageModal}>
          Create debug package
        </Button>
        <ButtonGroup isAttached>
          <Button
            size="sm"
            colorScheme="green"
            onClick={() => onExecute({ queryValue, queryBindParams })}
          >
            Execute
          </Button>
          <Menu>
            <MenuButton
              as={IconButton}
              size="sm"
              colorScheme="green"
              aria-label="Query execution options"
              icon={<ChevronDownIcon />}
            />
            <MenuList zIndex="500">
              <MenuItem
                onClick={() => onProfile({ queryValue, queryBindParams })}
              >
                Profile
              </MenuItem>
              <MenuItem
                onClick={() => onExplain({ queryValue, queryBindParams })}
              >
                Explain
              </MenuItem>
            </MenuList>
          </Menu>
        </ButtonGroup>
      </Stack>
    </Flex>
  );
};
