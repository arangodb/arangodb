import { CloseIcon } from "@chakra-ui/icons";
import { Box, Button, FormLabel, IconButton } from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { SelectControl } from "../../../../components/form/SelectControl";
import { AddNewViewFormValues } from "./AddNewViewForm.types";
import { useCollectionsList } from "./useCollectionsList";
import { useInvertedIndexList } from "./useInvertedIndexList";

export const IndexesForm = () => {
  const { collectionsList } = useCollectionsList();
  const collectionsOptions = collectionsList?.map(collection => {
    return { value: collection.name, label: collection.name };
  });
  const { values } = useFormikContext<AddNewViewFormValues>();
  return (
    <Box marginTop="5">
      <FieldArray name="indexes">
        {({ remove, push }) => (
          <Box display={"grid"} rowGap="4">
            {values.indexes.map((value, index) => {
              return (
                <CollectionIndexRow
                  value={value}
                  index={index}
                  collectionsOptions={collectionsOptions}
                  remove={remove}
                />
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
const CollectionIndexRow = ({
  value,
  index,
  collectionsOptions,
  remove
}: {
  value: { collection: string; index: string };
  index: number;
  collectionsOptions: { value: string; label: string }[] | undefined;
  remove: <T>(index: number) => T | undefined;
}) => {
  const { collection } = value;
  const { indexesList } = useInvertedIndexList(collection);

  const indexesOptions = indexesList?.map(indexItem => {
    return { value: indexItem.name, label: indexItem.name };
  });
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
        <FormLabel htmlFor={`indexes.${index}.index`}>Index</FormLabel>
        <SelectControl
          name={`indexes.${index}.index`}
          selectProps={{
            options: indexesOptions
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
};
