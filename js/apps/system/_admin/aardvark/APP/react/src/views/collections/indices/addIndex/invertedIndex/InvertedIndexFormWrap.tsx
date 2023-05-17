import { Box } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { Split, SplitDivider } from "../../../../../components/split/Split";
import { FormFieldProps } from "../FormField";
import { FormActions } from "../IndexFormFieldList";
import { InvertedIndexProvider } from "./InvertedIndexContext";
import { InvertedIndexFormJSONEditor } from "./InvertedIndexFormJSONEditor";
import { InvertedIndexLeftPane } from "./InvertedIndexLeftPane";
import {
  InvertedIndexValuesType,
  useCreateInvertedIndex
} from "./useCreateInvertedIndex";

export const InvertedIndexFormWrap = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateInvertedIndex();
  return (
    <InvertedIndexForm
      onCreate={onCreate}
      initialValues={initialValues}
      schema={schema}
      fields={fields}
      onClose={onClose}
    />
  );
};
export const InvertedIndexForm = ({
  onCreate,
  initialValues,
  schema,
  fields,
  onClose,
  isFormDisabled
}: {
  onCreate?: ({ values }: { values: InvertedIndexValuesType }) => Promise<void>;
  initialValues: InvertedIndexValuesType;
  schema?: any;
  fields: FormFieldProps[];
  onClose: () => void;
  isFormDisabled?: boolean;
}) => {
  return (
    <InvertedIndexProvider>
      <Formik
        onSubmit={async values => {
          await onCreate?.({ values });
        }}
        initialValues={initialValues}
        validationSchema={schema}
        isInitialValid={false}
      >
        {() => (
          <Box as={Form} height="calc(100% - 30px)">
            <Split
              storageKey={"invertedIndexJSONSplitTemplate"}
              render={({ getGridProps, getGutterProps }) => {
                const gridProps = getGridProps();
                const gutterProps = getGutterProps("column", 1);
                return (
                  <Box display="grid" {...gridProps}>
                    <InvertedIndexLeftPane
                      isFormDisabled={isFormDisabled}
                      fields={fields}
                    />
                    <SplitDivider gutterProps={gutterProps} />
                    <InvertedIndexFormJSONEditor
                      isFormDisabled={isFormDisabled}
                    />
                  </Box>
                );
              }}
            />

            <FormActions isFormDisabled={isFormDisabled} onClose={onClose} />
          </Box>
        )}
      </Formik>
    </InvertedIndexProvider>
  );
};
