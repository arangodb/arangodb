import { Grid } from "@chakra-ui/react";
import React from "react";
import { EditViewProvider } from "../editView/EditViewContext";
import { EditViewHeader } from "../editView/EditViewHeader";
import { SearchAliasViewPropertiesType } from "../View.types";
import { SearchAliasJsonForm } from "./SearchAliasJsonForm";
import { useUpdateAliasViewProperties } from "./useUpdateAliasViewProperties";
import { useNavbarHeight } from "../../useNavbarHeight";

export const SearchAliasViewSettings = ({
  view
}: {
  view: SearchAliasViewPropertiesType;
}) => {
  const { onSave } = useUpdateAliasViewProperties();
  const navbarHeight = useNavbarHeight();
  return (
    <EditViewProvider initialView={view} onSave={onSave as any}>
      <Grid
        width="full"
        height={`calc(100vh - ${navbarHeight}px)`}
        gridTemplateRows="120px 1fr"
        rowGap="5"
      >
        <EditViewHeader />
        <SearchAliasJsonForm />
      </Grid>
    </EditViewProvider>
  );
};
