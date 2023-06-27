import { AddIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Button,
  FormLabel,
  Grid,
  GridItem,
  IconButton,
  Spacer,
  Stack,
  Text
} from "@chakra-ui/react";
import { CollectionType } from "arangojs";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import useSWR from "swr";
import { CreatableSingleSelectControl } from "../../../components/form/CreatableSingleSelectControl";
import { FormField } from "../../../components/form/FormField";
import { getCurrentDB } from "../../../utils/arangoClient";
import { IndexInfoTooltip } from "../../collections/indices/addIndex/IndexInfoTooltip";
import { GeneralGraphCreateValues } from "./CreateGraph.types";

const graphRelationFieldsMap = {
  collection: {
    name: "collection",
    type: "custom",
    label: "edgeDefinition",
    tooltip: "An edge definition defines a relation of the graph.",
    isRequired: true
  },
  from: {
    name: "from",
    type: "custom",
    label: "fromCollections",
    tooltip: "The collections that contain the start vertices of the relation.",
    isRequired: true
  },
  to: {
    name: "to",
    type: "custom",
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
export const EdgeDefinitionsField = () => {
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
                      <CollectionField index={index} />
                      <FormField
                        field={{
                          ...graphRelationFieldsMap.from,
                          name: `edgeDefinitions[${index}]${graphRelationFieldsMap.from.name}`
                        }}
                      />
                      <FormField
                        field={{
                          ...graphRelationFieldsMap.to,
                          name: `edgeDefinitions[${index}]${graphRelationFieldsMap.to.name}`
                        }}
                      />
                    </Grid>
                  </Stack>
                );
              })}
              <Button
                onClick={() => {
                  push({
                    from: "",
                    to: "",
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

const useFetchCollections = () => {
  const currentDB = getCurrentDB();
  const fetchCollections = async () => {
    const collections = await currentDB.listCollections();
    return collections;
  };

  const { data } = useSWR("/collections", fetchCollections);
  return { collections: data };
};

const useEdgeCollectionOptions = () => {
  const { collections } = useFetchCollections()
  const edgeCollectionOptions = collections
  ?.filter(collection => {
    return collection.type === CollectionType.EDGE_COLLECTION;
  })
  .map(collection => {
    return {
      value: collection.name,
      label: collection.name
    };
  });

  return edgeCollectionOptions

}
const CollectionField = ({ index }: { index: number }) => {
  const edgeCollectionOptions = useEdgeCollectionOptions()
  return (
    <FormField
      field={{
        ...graphRelationFieldsMap.collection,
        name: `edgeDefinitions[${index}]${graphRelationFieldsMap.collection.name}`
      }}
      render={props => {
        const { field } = props;
        return (
          <>
            <FormLabel margin="0" htmlFor={field.name}>
              {field.label}
            </FormLabel>
            <CreatableSingleSelectControl
              selectProps={{
                isClearable: true,
                options: edgeCollectionOptions
              }}
              name={field.name}
            />
            {field.tooltip ? (
              <IndexInfoTooltip label={field.tooltip} />
            ) : (
              <Spacer />
            )}
          </>
        );
      }}
    />
  );
};
