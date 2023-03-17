import { Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import SplitPane from "react-split-pane";
import useElementSize from "../../../views/useElementSize";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { InvertedIndexProvider } from "./InvertedIndexContext";
import { InvertedIndexFieldsDataEntry } from "./InvertedIndexFieldsDataEntry";
import { InvertedIndexFormJSONEditor } from "./InvertedIndexFormJSONEditor";
import { useCreateInvertedIndex } from "./useCreateInvertedIndex";

export const InvertedIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateInvertedIndex();
  const [sectionRef, sectionSize] = useElementSize();
  const sectionWidth = sectionSize.width;
  const maxSize = sectionWidth - 200;
  const localStorageSplitPos = localStorage.getItem("splitPos") || "400";
  let splitPos = parseInt(localStorageSplitPos, 10);
  if (splitPos > sectionWidth - 200) {
    splitPos = sectionWidth - 200;
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
          <Form>
            <Stack spacing="4" ref={sectionRef}>
              <Stack direction="row" height="100vh">
                <SplitPane
                  paneStyle={{ overflow: "auto" }}
                  maxSize={maxSize}
                  defaultSize={splitPos}
                  onChange={size =>
                    localStorage.setItem("splitPos", size.toString())
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
                </SplitPane>
              </Stack>
              <FormActions onClose={onClose} />
            </Stack>
          </Form>
        )}
      </Formik>
    </InvertedIndexProvider>
  );
};
