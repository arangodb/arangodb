import { Box } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import Split from "react-split";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { InvertedIndexProvider } from "./InvertedIndexContext";
import { InvertedIndexFieldsDataEntry } from "./InvertedIndexFieldsDataEntry";
import { InvertedIndexFormJSONEditor } from "./InvertedIndexFormJSONEditor";
import { useCreateInvertedIndex } from "./useCreateInvertedIndex";

export const InvertedIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateInvertedIndex();
  const localStorageSplitSize = localStorage.getItem(
    "invertedIndexJSONSplitSizes"
  );
  let sizes = [50, 50];
  try {
    sizes = localStorageSplitSize ? JSON.parse(localStorageSplitSize) : sizes;
  } catch {
    // ignore error
  }
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
              style={{
                display: "flex",
                height: "100%"
              }}
              sizes={sizes}
              onDragEnd={sizes =>
                localStorage.setItem(
                  "invertedIndexJSONSplitSizes",
                  JSON.stringify(sizes)
                )
              }
            >
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
                  return <>{field.name}</>;
                }}
              />
              <InvertedIndexFormJSONEditor />
            </Split>
            <FormActions onClose={onClose} />
          </Box>
        )}
      </Formik>
    </InvertedIndexProvider>
  );
};
