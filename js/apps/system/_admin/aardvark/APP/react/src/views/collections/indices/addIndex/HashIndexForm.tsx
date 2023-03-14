import { Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";
import { useCreateHashIndex } from "./useCreateHashIndex";

export const HashIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateHashIndex();

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
