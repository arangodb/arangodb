import { Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { IndexFormFieldsList, FormActions } from "./IndexFormFieldList";
import { useCreateFulltextIndex } from "./useCreateFulltextIndex";

export const FulltextIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateFulltextIndex();

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


