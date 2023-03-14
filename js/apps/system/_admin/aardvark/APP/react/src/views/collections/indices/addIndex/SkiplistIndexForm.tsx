import { Box, Button, FormLabel, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { useCreateSkiplistIndex } from "./useCreateSkiplistIndex";

export const SkiplistIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema } = useCreateSkiplistIndex();

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
              <FormLabel htmlFor="unique">Unique</FormLabel>
              <SwitchControl name="unique" />
              <FormLabel htmlFor="sparse">Sparse</FormLabel>
              <SwitchControl name="sparse" />
              <FormLabel htmlFor="deduplicate">
                Deduplicate array values
              </FormLabel>
              <SwitchControl name="deduplicate" />
              <FormLabel htmlFor="estimates">
                Maintain index selectivity estimates
              </FormLabel>
              <SwitchControl name="estimates" />
              <FormLabel htmlFor="cacheEnabled">
                Enable in-memory cache for index lookups
              </FormLabel>
              <SwitchControl name="cacheEnabled" />
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
