import { Box } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React, { useState } from "react";
import Split from "react-split-grid";
import { FormActions, IndexFormFieldsList } from "../IndexFormFieldList";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { InvertedIndexProvider } from "./InvertedIndexContext";
import { InvertedIndexFieldsDataEntry } from "./InvertedIndexFieldsDataEntry";
import { InvertedIndexFormJSONEditor } from "./InvertedIndexFormJSONEditor";
import { InvertedIndexPrimarySort } from "./InvertedIndexPrimarySort";
import { InvertedIndexStoredValues } from "./InvertedIndexStoredValues";
import { InvertedIndexConsolidationPolicy } from "./InvertedIndexConsolidationPolicy";
import { useCreateInvertedIndex } from "./useCreateInvertedIndex";

export const InvertedIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateInvertedIndex();
  const localStorageSplitSize = localStorage.getItem(
    "invertedIndexJSONSplitTemplate"
  );
  const [gridTemplateColumns, setGridTemplateColumns] = useState(
    localStorageSplitSize || "1fr 10px 1fr"
  );
  return (
    <InvertedIndexProvider>
      <Formik
        onSubmit={async values => {
          await onCreate({ values });
        }}
        initialValues={initialValues}
        validationSchema={schema}
        isInitialValid={false}
      >
        {() => (
          <Box as={Form} height="calc(100% - 30px)">
            <Split
              gridTemplateColumns={gridTemplateColumns}
              minSize={100}
              cursor="col-resize"
              onDrag={(_direction, _track, gridTemplateStyle) => {
                setGridTemplateColumns(gridTemplateStyle);
                localStorage.setItem(
                  "invertedIndexJSONSplitTemplate",
                  gridTemplateStyle
                );
              }}
              render={({ getGridProps, getGutterProps }) => {
                const gridProps = getGridProps();
                const gutterProps = getGutterProps("column", 1);
                return (
                  <Box display="grid" {...gridProps}>
                    <IndexFormFieldsList
                      fields={fields}
                      renderField={({ field, autoFocus }) => {
                        if (field.name === "analyzer") {
                          return (
                            <InvertedIndexAnalyzerDropdown
                              field={field}
                              autoFocus={autoFocus}
                            />
                          );
                        }
                        if (field.name === "fields") {
                          return (
                            <InvertedIndexFieldsDataEntry
                              field={field}
                              autoFocus={autoFocus}
                            />
                          );
                        }
                        if (field.name === "primarySort") {
                          return <InvertedIndexPrimarySort field={field} />;
                        }
                        if (field.name === "storedValues") {
                          return <InvertedIndexStoredValues field={field} />;
                        }
                        if (field.name === "consolidationPolicy") {
                          return <InvertedIndexConsolidationPolicy field={field} />;
                        }
                        return <>{field.name}</>;
                      }}
                    />
                    <Box
                      gridRow="1/-1"
                      backgroundColor="gray.300"
                      cursor="col-resize"
                      gridColumn="2"
                      position="relative"
                      {...gutterProps}
                    ></Box>

                    <InvertedIndexFormJSONEditor />
                  </Box>
                );
              }}
            />

            <FormActions onClose={onClose} />
          </Box>
        )}
      </Formik>
    </InvertedIndexProvider>
  );
};
