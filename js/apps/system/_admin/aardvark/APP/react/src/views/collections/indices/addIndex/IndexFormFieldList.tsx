import { Box, Button, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { IndexFormField, IndexFormFieldProps } from "./IndexFormField";

export const IndexFormFieldsList = ({
  fields
}: {
  fields: IndexFormFieldProps[];
}) => {
  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      columnGap="3"
      maxWidth="500px"
    >
      {fields.map((field, index) => {
        return <IndexFormField key={field.name} index={index} field={field} />;
      })}
    </Box>
  );
};
export const FormActions = ({ onClose }: { onClose: () => void }) => {
  const { isValid, isSubmitting } = useFormikContext();
  return (
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
  );
};
