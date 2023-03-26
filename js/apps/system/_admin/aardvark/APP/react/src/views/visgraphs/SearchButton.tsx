import { SearchIcon } from "@chakra-ui/icons";
import {
  Box,
  FormLabel, IconButton,
  Menu,
  MenuButton,
  MenuList,
  Tooltip
} from "@chakra-ui/react";
import React from "react";
import SelectBase from "../../components/select/SelectBase";
import { useGraph } from "./GraphContext";

export const SearchButton = () => {
  const { datasets, network } = useGraph();
  const nodesList = datasets?.nodes.getIds();
  const options = nodesList?.map(nodeId => {
    return { value: nodeId as string, label: nodeId as string };
  });
  return (
    <Menu closeOnSelect={false} closeOnBlur={false}>
      <Tooltip hasArrow label={"Search nodes"} placement="bottom">
        <MenuButton
          as={IconButton}
          size="sm"
          icon={<SearchIcon />}
          aria-label={"Search nodes"} />
      </Tooltip>
      <MenuList>
        <Box padding={"4"}>
          <FormLabel htmlFor="nodeSearch">Search for a node</FormLabel>
          <SelectBase
            inputId="nodeSearch"
            placeholder="Start typing to search in nodes"
            options={options}
            onChange={value => {
              network?.fit({
                nodes: [(value as any).value]
              });
            }} />
        </Box>
      </MenuList>
    </Menu>
  );
};
