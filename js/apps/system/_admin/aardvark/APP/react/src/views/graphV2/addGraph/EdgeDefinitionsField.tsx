import { AddIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Button,
  Grid,
  GridItem,
  IconButton,
  Stack,
  Text
} from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { FormField } from "../../../components/form/FormField";
import { GeneralGraphCreateValues } from "./CreateGraph.types";
import { useCollectionOptions } from "./useEdgeCollectionOptions";
import { useResetFromAndToValues } from "./useResetFromAndToValues";

const graphRelationFieldsMap = {
  collection: {
    name: "collection",
    type: "creatableSingleSelect",
    label: "edgeDefinition",
    tooltip: "An edge definition defines a relation of the graph.",
    isRequired: true
  },
  from: {
    name: "from",
    type: "creatableMultiSelect",
    label: "fromCollections",
    tooltip: "The collections that contain the start vertices of the relation.",
    isRequired: true
  },
  to: {
    name: "to",
    type: "creatableMultiSelect",
    label: "toCollections",
    tooltip: "The collections that contain the end vertices of the relation.",
    isRequired: true
  }
};

/**
 * behaviour required
 * - Don't allow multiple edge defs with the same name
 * - If an edge def already exists, fill from/to & disable the inputs
 * - autocomplete edge defs
 */

export const EdgeDefinitionsField = ({
  noOptionsMessage,
  allowExistingCollections = true
}: {
  noOptionsMessage?: () => string;
  allowExistingCollections?: boolean;
}) => {
  const { values } = useFormikContext<GeneralGraphCreateValues>();
  return (
    <GridItem colSpan={3}>
      <FieldArray name="edgeDefinitions">
        {({ remove, push }) => {
          return (
            <Stack spacing="4">
              <Text fontWeight="medium" fontSize="lg">
                Relations
              </Text>
              {values.edgeDefinitions.map((_edgeDef, index) => {
                return (
                  <EdgeDefinition
                    key={index}
                    index={index}
                    remove={remove}
                    allowExistingCollections={allowExistingCollections}
                    noOptionsMessage={noOptionsMessage}
                  />
                );
              })}
              <Button
                onClick={() => {
                  push({
                    from: [],
                    to: [],
                    collection: ""
                  });
                }}
                variant="ghost"
                colorScheme="blue"
                leftIcon={<AddIcon />}
              >
                Add relation
              </Button>
            </Stack>
          );
        }}
      </FieldArray>
    </GridItem>
  );
};

const EdgeDefinition = ({
  index,
  remove,
  allowExistingCollections,
  noOptionsMessage
}: {
  index: number;
  remove: <T>(index: number) => T | undefined;
  allowExistingCollections: boolean;
  noOptionsMessage: (() => string) | undefined;
}) => {
  const { collectionToDisabledMap } = useResetFromAndToValues();
  const { edgeCollectionOptions, documentCollectionOptions } =
    useCollectionOptions();
  const isFromAndToDisabled = collectionToDisabledMap[index];
  const collectionFieldName = `edgeDefinitions[${index}]${graphRelationFieldsMap.collection.name}`;
  const fromFieldName = `edgeDefinitions[${index}]${graphRelationFieldsMap.from.name}`;
  const toFieldName = `edgeDefinitions[${index}]${graphRelationFieldsMap.to.name}`;
  return (
    <Stack
      spacing="4"
      backgroundColor="gray.100"
      padding="4"
      borderRadius={"md"}
    >
      <Stack direction="row" alignItems="center" spacing="4">
        <Text fontWeight="medium" fontSize="lg">
          Relation {index + 1}
        </Text>
        {index !== 0 && (
          <IconButton
            size="sm"
            variant="ghost"
            colorScheme="red"
            icon={<DeleteIcon />}
            onClick={() => {
              remove(index);
            }}
            aria-label={"Remove relation"}
          />
        )}
      </Stack>
      <Grid gridTemplateColumns={"200px 1fr 40px"} gap="4">
        <FormField
          field={{
            ...graphRelationFieldsMap.collection,
            options: allowExistingCollections
              ? edgeCollectionOptions
              : undefined,
            isClearable: true,
            name: collectionFieldName,
            noOptionsMessage
          }}
        />
        <FormField
          field={{
            ...graphRelationFieldsMap.from,
            name: fromFieldName,
            isDisabled: isFromAndToDisabled,
            options: allowExistingCollections
              ? documentCollectionOptions
              : undefined,
            noOptionsMessage
          }}
        />
        <FormField
          field={{
            ...graphRelationFieldsMap.to,
            name: toFieldName,
            isDisabled: isFromAndToDisabled,
            options: allowExistingCollections
              ? documentCollectionOptions
              : undefined,
            noOptionsMessage
          }}
        />
      </Grid>
    </Stack>
  );
};
