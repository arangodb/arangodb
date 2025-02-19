import { Box, Grid } from "@chakra-ui/react";
import { Form, useFormikContext } from "formik";
import React, { useEffect } from "react";
import { Split, SplitDivider } from "../../../components/split/Split";
import {
  EditViewProvider,
  useEditViewContext
} from "../editView/EditViewContext";
import { EditViewHeader } from "../editView/EditViewHeader";
import { ArangoSearchViewPropertiesType } from "../View.types";
import { ArangoSearchJSONEditor } from "./ArangoSearchJSONEditor";
import { ArangoSearchViewForm } from "./ArangoSearchViewForm";
import { useUpdateArangoSearchViewProperties } from "./useUpdateArangoSearchViewProperties";
import { useNavbarHeight } from "../../useNavbarHeight";

export const ArangoSearchViewSettings = ({
  view
}: {
  view: ArangoSearchViewPropertiesType;
}) => {
  const { onSave } = useUpdateArangoSearchViewProperties();
  return (
    <EditViewProvider initialView={view} onSave={onSave}>
      <ArangoSearchViewSettingsInner />
    </EditViewProvider>
  );
};
export const ArangoSearchViewSettingsInner = () => {
  const { setChanged } = useEditViewContext();
  const { values, dirty } = useFormikContext();
  useEffect(() => {
    if (dirty) {
      setChanged(true);
    }
  }, [values, setChanged, dirty]);
  const navbarHeight = useNavbarHeight();
  return (
    <Grid
      as={Form}
      onChange={() => {
        setChanged(true);
      }}
      backgroundColor="white"
      width="100%"
      height={`calc(100vh - ${navbarHeight}px)`}
      gridTemplateRows="120px 1fr"
      marginBottom="0"
    >
      <EditViewHeader />
      <Split
        storageKey={"viewJSONSplitTemplate"}
        render={({ getGridProps, getGutterProps }) => {
          const gridProps = getGridProps();
          const gutterProps = getGutterProps("column", 1);
          return (
            <Box display="grid" height="full" {...gridProps}>
              <ArangoSearchViewForm />
              <SplitDivider gutterProps={gutterProps} />
              <ArangoSearchJSONEditor />
            </Box>
          );
        }}
      />
    </Grid>
  );
};
