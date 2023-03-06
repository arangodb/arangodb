import { Box } from "@chakra-ui/react";
import React from "react";
import { useHistory } from "react-router-dom";
import { SearchViewType } from "./useViewsList";

export const ViewTile = ({ view }: { view: SearchViewType }) => {
  const history = useHistory();
  return (
    <Box
      display="flex"
      flexDirection="column"
      height="100px"
      boxShadow="md"
      backgroundColor="white"
      cursor="pointer"
      onClick={() => history.push(`/view/${view.name}`)}
    >
      <Box as="i" fontSize={"4xl"} className="fa fa-clone" margin={"auto"} />
      <Box
        backgroundColor="gray.700"
        color="white"
        fontSize="sm"
        padding="1"
        display="flex"
        alignItems={"center"}
      >
        <Box minWidth={0}>{view.name}</Box>
        <Box
          marginLeft="auto"
          paddingX="2"
          borderRadius={"sm"}
          backgroundColor={"cyan.500"}
        >
          {view.type}
        </Box>
      </Box>
    </Box>
  );
};
