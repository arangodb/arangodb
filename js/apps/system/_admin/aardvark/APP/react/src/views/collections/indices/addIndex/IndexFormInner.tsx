import { Stack } from "@chakra-ui/react";
import { Form, Formik, FormikValues } from "formik";
import React from "react";
import { IndexFormFieldProps } from "./IndexFormField";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";

interface IndexFormInnerProps<InitialValues, Fields, Schema> {
  onCreate: ({ values }: { values: InitialValues }) => Promise<void>;
  initialValues: InitialValues;
  schema: Schema;
  fields: Fields;
  onClose: () => void;
}
export const IndexFormInner = <
  InitialValues extends FormikValues,
  Fields extends IndexFormFieldProps[],
  Schema extends unknown
>({
  onCreate,
  initialValues,
  schema,
  fields,
  onClose
}: IndexFormInnerProps<InitialValues, Fields, Schema>) => {
  return (
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
          <Stack spacing="4">
            <IndexFormFieldsList fields={fields} />
            <FormActions onClose={onClose} />
          </Stack>
        </Form>
      )}
    </Formik>
  );
};
