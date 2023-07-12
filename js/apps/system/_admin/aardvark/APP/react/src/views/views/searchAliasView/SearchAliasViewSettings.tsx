import { Box } from "@chakra-ui/react";
import React from "react";
import { EditViewProvider } from "../editView/EditViewContext";
import { EditViewHeader } from "../editView/EditViewHeader";
import { SearchAliasViewPropertiesType } from "../searchView.types";
import { SearchAliasJsonForm } from "./SearchAliasJsonForm";
import { useUpdateAliasViewProperties } from "./useUpdateAliasViewProperties";

export const SearchAliasViewSettings = ({
  view
}: {
  view: SearchAliasViewPropertiesType;
}) => {
  const { onSave } = useUpdateAliasViewProperties();
  return (
    <EditViewProvider initialView={view} onSave={onSave as any}>
      <SearchAliasViewSettingsInner />
    </EditViewProvider>
  );
};

const SearchAliasViewSettingsInner = () => {
  return (
    <Box
      width="full"
      height="calc(100vh - 60px)"
      display="grid"
      gridTemplateRows="120px 1fr"
      rowGap="5"
    >
      <EditViewHeader />
      <SearchAliasJsonForm />
    </Box>
  );
};
