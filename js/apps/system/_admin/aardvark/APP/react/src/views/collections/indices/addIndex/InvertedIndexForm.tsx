import { Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";
import { useCreateInvertedIndex } from "./useCreateInvertedIndex";

export const InvertedIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateInvertedIndex();

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
