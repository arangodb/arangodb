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
  Text
} from "@chakra-ui/react";
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

      <Text fontWeight="medium">
        Query name:{" "}
        {existingQuery && currentQueryName ? currentQueryName : "Untitled"}
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
        <ActionButton
          actions={{
            execute: {
              method: () =>
                onExecute({
                  queryValue,
                  queryBindParams,
                  queryOptions,
                  disabledRules
                }),
              label: "Execute"
            },
            profile: {
              method: () =>
                onProfile({
                  queryValue,
                  queryBindParams,
                  queryOptions,
                  disabledRules
                }),
              label: "Profile"
            }
          }}
        />
      </Stack>
    </Flex>
  );
};

const ActionButton = ({
  actions,
  initialDefaultAction = "execute"
}: {
  actions: {
    [key: string]: {
      method: () => void;
      label: string;
    };
  };
  initialDefaultAction?: string;
}) => {
  const [defaultAction, setDefaultAction] =
    React.useState<string>(initialDefaultAction);
  const restActions = Object.keys(actions).filter(
    action => action !== defaultAction
  );
  return (
    <ButtonGroup isAttached>
      <Button
        size="sm"
        colorScheme="green"
        onClick={actions[defaultAction].method}
      >
        {actions[defaultAction].label}
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
          {restActions.map(
            (action, index) =>
              actions[action] && (
                <MenuItem
                  key={index}
                  onClick={() => {
                    setDefaultAction(action);
                    actions[action].method();
                  }}
                >
                  {actions[action].label}
                </MenuItem>
              )
          )}
        </MenuList>
      </Menu>
    </ButtonGroup>
  );
};
