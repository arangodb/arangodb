import { Box, Button, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { IndexFormField, IndexFormFieldProps } from "./IndexFormField";

export const IndexFormFieldsList = ({
  fields,
  renderField
}: {
  fields: IndexFormFieldProps[];
  renderField?: (props: {
    field: IndexFormFieldProps;
    index?: number;
    autoFocus: boolean;
  }) => JSX.Element;
}) => {
  return (
    <Box minWidth="0">
      <Box
        display={"grid"}
        gridTemplateColumns={"200px 1fr 40px"}
        rowGap="5"
        columnGap="3"
        maxWidth="800px"
        paddingRight="8"
        paddingLeft="10"
        alignItems="center"
        marginTop="4"
      >
        {fields.map((field, index) => {
          return (
            <IndexFormField
              render={renderField}
              key={field.name}
              index={index}
              field={field}
            />
          );
        })}
      </Box>
    </Box>
  );
};
export const FormActions = ({ onClose }: { onClose: () => void }) => {
  const { isValid, isSubmitting } = useFormikContext();
  return (
    <Stack marginY="2" direction="row" justifyContent="flex-end" padding="10">
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
