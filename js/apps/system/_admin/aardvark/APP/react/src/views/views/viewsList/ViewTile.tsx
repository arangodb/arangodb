import { Box } from "@chakra-ui/react";
import React from "react";
import { useHistory } from "react-router-dom";
import { SearchViewTypeWithLock } from "./useViewsList";

export const ViewTile = ({ view }: { view: SearchViewTypeWithLock }) => {
  const history = useHistory();
  return (
    <Box
      display="flex"
      flexDirection="column"
      height="100px"
      boxShadow="md"
      backgroundColor="white"
      cursor="pointer"
      onClick={() => history.push(`/view/${encodeURIComponent(view.name)}`)}
      title={view.name}
    >
      <Box
        as="i"
        fontSize={"4xl"}
        className={`fa ${view.isLocked ? "fa-spinner fa-spin" : "fa-clone"}`}
        margin={"auto"}
      />
      <Box
        backgroundColor="gray.700"
        color="white"
        fontSize="sm"
        padding="1"
        display="flex"
        alignItems={"center"}
      >
        <Box
          minWidth={0}
          textOverflow="ellipsis"
          overflow="hidden"
          whiteSpace="nowrap"
        >
          {view.name}
        </Box>
        <Box
          marginLeft="auto"
          paddingX="2"
          borderRadius={"sm"}
          backgroundColor={"cyan.500"}
          title={view.type}
          textOverflow="ellipsis"
          overflow="hidden"
          whiteSpace="nowrap"
        >
          {view.type}
        </Box>
      </Box>
    </Box>
  );
};
