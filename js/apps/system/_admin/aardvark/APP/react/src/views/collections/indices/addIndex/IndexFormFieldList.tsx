import { Box, Button, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { FormField, FormFieldProps } from "./FormField";

export const IndexFormFieldsList = ({
  fields,
  isFormDisabled,
  renderField
}: {
  fields: FormFieldProps[];
  isFormDisabled?: boolean;
  renderField?: (props: {
    field: FormFieldProps;
    index?: number;
    autoFocus: boolean;
  }) => JSX.Element;
}) => {
  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      columnGap="3"
      maxWidth="800px"
      paddingRight="16"
      paddingLeft="10"
      alignItems="center"
      marginTop="4"
    >
      {fields.map((field, index) => {
        return (
          <FormField
            render={renderField}
            key={field.name}
            index={index}
            field={{
              ...field,
              isDisabled: field.isDisabled || isFormDisabled
            }}
          />
        );
      })}
    </Box>
  );
};
export const FormActions = ({
  onClose,
  isFormDisabled
}: {
  onClose: () => void;
  isFormDisabled?: boolean;
}) => {
  const { isValid, isSubmitting } = useFormikContext();
  return (
    <Stack marginY="2" direction="row" justifyContent="flex-end" padding="10">
      <Button size="sm" onClick={onClose}>
        Close
      </Button>
      {!isFormDisabled && (
        <Button
          colorScheme="green"
          size="sm"
          type="submit"
          isDisabled={!isValid}
          isLoading={isSubmitting}
        >
          Create
        </Button>
      )}
    </Stack>
  );
};
