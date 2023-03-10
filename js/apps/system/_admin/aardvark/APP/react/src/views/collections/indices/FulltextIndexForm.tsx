import { Box, Button, FormLabel, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import { InputControl } from "../../../components/form/InputControl";
import { SwitchControl } from "../../../components/form/SwitchControl";

export const FulltextIndexForm = ({ onClose }: { onClose: () => void }) => {
  return (
    <Formik
      onSubmit={() => {}}
      initialValues={{
        type: "fulltext",
        fields: "",
        minLength: 0,
        inBackground: true,
        name: ""
      }}
      validationSchema={Yup.object({
        fields: Yup.string().required("Fields are required")
      })}
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
              <InputControl name="minLength" />
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
