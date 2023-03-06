import { CloseIcon } from "@chakra-ui/icons";
import { Box, Button, FormLabel, IconButton } from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { SelectControl } from "../../../components/form/SelectControl";
import { AddNewViewFormValues } from "./AddNewViewForm.types";
import { useCollectionsList } from "./useCollectionsList";

export const IndexesForm = () => {
  const { collectionsList } = useCollectionsList();

  console.log({ collectionsList });
  const collectionsOptions = collectionsList?.map(collection => {
    return { value: collection.name, label: collection.name };
  });
  const { values } = useFormikContext<AddNewViewFormValues>();
  return (
    <Box>
      <FieldArray name="indexes">
        {({ remove, push }) => (
          <Box display={"grid"} rowGap="4">
            {values.indexes.map((value, index) => {
              const { collection } = value;
              const indexOptions = [{
                value: '',
                label: ''
              }];
              console.log({ collection });

              return (
                <Box
                  display={"grid"}
                  gridColumnGap="4"
                  gridTemplateColumns={"1fr 1fr 40px"}
                  rowGap="5"
                  alignItems={"end"}
                  key={index}
                >
                  <Box>
                    <FormLabel htmlFor={`indexes.${index}.collection`}>
                      Collection
                    </FormLabel>
                    <SelectControl
                      name={`indexes.${index}.collection`}
                      selectProps={{
                        options: collectionsOptions
                      }}
                    />
                  </Box>
                  <Box>
                    <FormLabel htmlFor={`indexes.${index}.collection`}>
                      Index
                    </FormLabel>
                    <SelectControl
                      name={`indexes.${index}.collection`}
                      selectProps={{
                        options: indexOptions
                      }}
                    />
                  </Box>
                  {index > 0 ? (
                    <IconButton
                      size="sm"
                      variant="ghost"
                      colorScheme="red"
                      aria-label="Remove field"
                      icon={<CloseIcon />}
                      onClick={() => remove(index)}
                    />
                  ) : null}
                </Box>
              );
            })}
            <Button
              colorScheme="blue"
              onClick={() => {
                push({ collection: "", index: "" });
              }}
              variant="ghost"
              justifySelf="start"
            >
              + Add field
            </Button>
          </Box>
        )}
      </FieldArray>
    </Box>
  );
};
