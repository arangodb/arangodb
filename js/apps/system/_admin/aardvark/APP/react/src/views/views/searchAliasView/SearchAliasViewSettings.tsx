import { Box } from "@chakra-ui/react";
import React from "react";
import { SearchAliasViewPropertiesType } from "../searchView.types";
import { SearchAliasProvider } from "./SearchAliasContext";
import { SearchAliasHeader } from "./SearchAliasHeader";
import { SearchAliasJsonForm } from "./SearchAliasJsonForm";

export const SearchAliasViewSettings = ({
  view
}: {
  view: SearchAliasViewPropertiesType;
}) => {
  return (
    <SearchAliasProvider initialView={view}>
      <Box
        width="full"
        height="calc(100vh - 60px)"
        display={"grid"}
        gridTemplateRows="120px 1fr"
        rowGap="5"
      >
        <SearchAliasHeader />
        <SearchAliasJsonForm />
      </Box>
    </SearchAliasProvider>
  );
};
