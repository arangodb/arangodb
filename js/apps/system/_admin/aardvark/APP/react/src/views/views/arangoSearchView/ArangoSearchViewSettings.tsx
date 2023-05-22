import { Box } from "@chakra-ui/react";
import React from "react";
import { Split, SplitDivider } from "../../../components/split/Split";
import { EditViewProvider } from "../editView/EditViewContext";
import { EditViewHeader } from "../editView/EditViewHeader";
import { ArangoSearchViewPropertiesType } from "../searchView.types";
import { ArangoSearchJSONEditor } from "./ArangoSearchJSONEditor";
import { ArangoSearchViewForm } from "./ArangoSearchViewForm";

export const ArangoSearchViewSettings = ({
  view
}: {
  view: ArangoSearchViewPropertiesType;
}) => {
  const onSave = async (data: any) => {
    console.log("onSave", data);
    return;
  };
  return (
    <EditViewProvider initialView={view} onSave={onSave as any}>
      <Box
        backgroundColor="white"
        width="full"
        height="calc(100vh - 60px)"
        display={"grid"}
        gridTemplateRows="120px 1fr"
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
      </Box>
    </EditViewProvider>
  );
};
