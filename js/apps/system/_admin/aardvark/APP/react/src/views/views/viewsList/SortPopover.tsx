import {
  Checkbox,
  IconButton,
  Popover,
  PopoverArrow,
  PopoverBody,
  PopoverCloseButton,
  PopoverContent,
  PopoverHeader,
  PopoverTrigger
} from "@chakra-ui/react";
import React from "react";
import { FilterList } from "styled-icons/material";

export const SortPopover = ({ toggleSort }: { toggleSort: () => void }) => {
  return (
    <Popover>
      <PopoverTrigger>
        <IconButton
          width="24px"
          height="24px"
          variant="ghost"
          size="xs"
          aria-label="Filter"
          icon={<FilterList />}
        />
      </PopoverTrigger>
      <PopoverContent boxShadow="dark-lg">
        <PopoverArrow />
        <PopoverCloseButton />
        <PopoverHeader>Sorting</PopoverHeader>
        <PopoverBody>
          <Checkbox
            onChange={() => {
              toggleSort();
            }}
          >
            Sort Descending
          </Checkbox>
        </PopoverBody>
      </PopoverContent>
    </Popover>
  );
};
