import { ChevronDownIcon } from "@chakra-ui/icons";
import {
  Button,
  ButtonGroup,
  IconButton,
  Menu,
  MenuButton,
  MenuItem,
  MenuList,
  Stack
} from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";

export const QueryBottomBar = () => {
  const {
    onExecute,
    onProfile,
    onExplain,
    queryValue,
    queryBindParams,
    queryResults,
    setQueryResults
  } = useQueryContext();
  return (
    <Stack
      direction={"row"}
      padding="2"
      justifyContent="flex-end"
      borderBottom="1px solid"
      borderX="1px solid"
      borderColor="gray.300"
    >
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
      <Button size="sm" colorScheme="gray">
        Create debug pacakge
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
          <MenuList>
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
  );
};
