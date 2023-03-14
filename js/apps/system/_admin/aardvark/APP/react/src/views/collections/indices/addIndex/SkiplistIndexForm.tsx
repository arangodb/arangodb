import { Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";
import { useCreateSkiplistIndex } from "./useCreateSkiplistIndex";

export const SkiplistIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateSkiplistIndex();

  return (
    <Formik
      onSubmit={async values => {
        await onCreate({ values });
      }}
      initialValues={initialValues}
      validationSchema={schema}
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
