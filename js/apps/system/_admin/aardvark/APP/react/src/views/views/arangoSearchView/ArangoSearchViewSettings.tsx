import { Box } from "@chakra-ui/react";
import { Formik } from "formik";
import React from "react";
import { Split, SplitDivider } from "../../../components/split/Split";
import { ArangoSearchViewPropertiesType } from "../searchView.types";
import { ArangoSearchPropertiesProvider } from "./ArangoSearchContext";
import { ArangoSearchViewForm } from "./ArangoSearchForm";
import { ArangoSearchJSONEditor } from "./ArangoSearchJSONEditor";

export const ArangoSearchViewSettings = ({
  view
}: {
  view: ArangoSearchViewPropertiesType;
}) => {
  const onCreate = async ({ values }: { values: any }) => {
    console.log("onCreate", values);
    return;
  };
  const initialValues = view;
  return (
    <ArangoSearchPropertiesProvider initialView={view}>
      <Formik
        onSubmit={async values => {
          await onCreate?.({ values });
        }}
        initialValues={initialValues}
        isInitialValid={false}
      >
        <Box
          backgroundColor="white"
          width="full"
          height="calc(100vh - 60px)"
          display={"grid"}
          gridTemplateRows="120px 1fr"
          rowGap="5"
        >
          <Box>Header</Box>
          <Box as="section" width="full" height="calc(100vh - 200px)">
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
        </Box>
      </Formik>
    </ArangoSearchPropertiesProvider>
  );
};
