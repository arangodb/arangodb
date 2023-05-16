import { Box, Spinner, Stack } from "@chakra-ui/react";
import React, { useState } from "react";
import { HashRouter } from "react-router-dom";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { AddViewTile } from "./AddViewTile";
import { SearchInput } from "./SearchInput";
import { SortPopover } from "./SortPopover";
import { useViewsList } from "./useViewsList";
import { ViewTile } from "./ViewTile";

export const ViewsList = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider overrideNonReact>
      <HashRouter basename="/" hashType={"noslash"}>
        <ViewsListInner />
      </HashRouter>
    </ChakraCustomProvider>
  );
};
const ViewsListInner = () => {
  const [searchValue, setSearchValue] = useState("");
  const [sortDescending, setSortDescending] = useState(false);
  const { viewsList, isLoading } = useViewsList({
    searchValue,
    sortDescending
  });
  if (!viewsList && isLoading) {
    return <Spinner />;
  }
  if (!viewsList) {
    return <>no views found</>;
  }
  return (
    <Box width="full" display={"grid"} gridTemplateRows="48px 1fr" rowGap="5">
      <Box
        backgroundColor="white"
        boxShadow="md"
        display="flex"
        alignItems="center"
        padding="4"
      >
        <Stack direction="row" marginLeft="auto" alignItems="center">
          <SortPopover
            toggleSort={() => {
              setSortDescending(sortOrder => !sortOrder);
            }}
          />
          <SearchInput
            onChange={event => {
              setSearchValue(event.target.value.normalize());
            }}
          />
        </Stack>
      </Box>
      <Box
        display={"grid"}
        gap="4"
        padding="4"
        gridAutoRows="100px"
        gridTemplateColumns={"repeat(auto-fill, minmax(190px, 1fr))"}
      >
        <AddViewTile />
        {viewsList.map(view => {
          return <ViewTile key={view.globallyUniqueId} view={view} />;
        })}
      </Box>
    </Box>
  );
};
