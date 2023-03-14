import { Box, Button, FormLabel, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { useCreateFulltextIndex } from "./useCreateFulltextIndex";

export const FulltextIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema } = useCreateFulltextIndex();

  return (
    <Formik
      onSubmit={async values => {
        await onCreate({ values });
      }}
      initialValues={initialValues}
      validationSchema={schema}
    >
      {({ isSubmitting, isValid }) => (
        <Form>
          <Stack spacing="4">
            <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
              <FormLabel htmlFor="fields">Fields</FormLabel>
              <InputControl isRequired name="fields" />
              <FormLabel htmlFor="name">Name</FormLabel>
              <InputControl name="name" />
              <FormLabel htmlFor="minLength">Min. Length</FormLabel>
              <InputControl
                inputProps={{
                  type: "number"
                }}
                name="minLength"
              />
              <FormLabel htmlFor="inBackground">Create in background</FormLabel>
              <SwitchControl name="inBackground" />
            </Box>
            <Stack direction="row" justifyContent="flex-end">
              <Button size="sm" onClick={onClose}>
                Close
              </Button>
              <Button
                colorScheme="green"
                size="sm"
                type="submit"
                isDisabled={!isValid}
                isLoading={isSubmitting}
              >
                Create
              </Button>
            </Stack>
          </Stack>
        </Form>
      )}
    </Formik>
  );
};
