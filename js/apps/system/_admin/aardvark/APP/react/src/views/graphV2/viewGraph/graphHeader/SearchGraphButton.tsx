import { SearchIcon } from "@chakra-ui/icons";
import {
  Box,
  FormLabel,
  IconButton,
  Menu,
  MenuButton,
  MenuList,
  Portal,
  Tooltip,
  useMenuContext
} from "@chakra-ui/react";
import React from "react";
import SingleSelect from "../../../../components/select/SingleSelect";
import { useGraph } from "../GraphContext";

export const SearchGraphButton = () => {
  return (
    <Menu closeOnSelect={false} closeOnBlur={false}>
      <Tooltip hasArrow label={"Search nodes"} placement="bottom">
        <MenuButton
          as={IconButton}
          size="sm"
          icon={<SearchIcon />}
          aria-label={"Search nodes"}
        />
      </Tooltip>
      <SearchList />
    </Menu>
  );
};
const SearchList = () => {
  const { datasets, network } = useGraph();
  const nodesList = datasets?.nodes.getIds();
  const options = nodesList?.map(nodeId => {
    return { value: nodeId as string, label: nodeId as string };
  });
  const { isOpen } = useMenuContext();

  if (!isOpen) {
    return null;
  }

  return (
    <Portal>
      <MenuList>
        <Box padding={"4"}>
          <FormLabel htmlFor="nodeSearch">Search for a node</FormLabel>
          <SingleSelect
            styles={{
              container: base => {
                return {
                  ...base,
                  width: "240px"
                };
              }
            }}
            inputId="nodeSearch"
            placeholder="Start typing to search in nodes"
            options={options}
            onChange={value => {
              network?.fit({
                nodes: [(value as any).value]
              });
            }}
          />
        </Box>
      </MenuList>
    </Portal>
  );
};
