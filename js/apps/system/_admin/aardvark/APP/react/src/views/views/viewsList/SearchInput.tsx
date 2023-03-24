import {
  Input,
  InputGroup,
  InputLeftElement,
  InputProps
} from "@chakra-ui/react";
import { SearchIcon } from "@chakra-ui/icons";
import React from "react";

export const SearchInput = (props: InputProps) => {
  return (
    <InputGroup size="sm">
      <InputLeftElement
        pointerEvents="none"
        children={<SearchIcon color="gray.300" />}
        />
      <Input placeholder="Search" {...props} />
    </InputGroup>
  );
};
