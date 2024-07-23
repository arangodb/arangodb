import { JSONSchemaType } from "ajv";
import { useEffect, useState } from "react";
import { useEditViewContext } from "../editView/EditViewContext";
import { SearchAliasViewPropertiesType } from "../View.types";

const searchAliasJsonSchema: JSONSchemaType<SearchAliasViewPropertiesType> = {
  $id: "https://arangodb.com/schemas/views/searchAliasViews.json",
  type: "object",
  properties: {
    id: {
      nullable: false,
      type: "string"
    },
    globallyUniqueId: {
      nullable: false,
      type: "string"
    },
    name: {
      nullable: false,
      type: "string"
    },
    type: {
      type: "string",
      const: "search-alias"
    },
    indexes: {
      type: "array",
      nullable: false,
      items: {
        type: "object",
        nullable: false,
        properties: {
          collection: {
            type: "string",
            nullable: false
          },
          index: {
            type: "string",
            nullable: false
          }
        },
        default: {
          collection: "",
          index: ""
        },
        required: ["collection", "index"],
        additionalProperties: false
      }
    }
  },
  required: ["id", "name", "type"],
  additionalProperties: false
};

export const useAliasViewSchema = ({
  view
}: {
  view: SearchAliasViewPropertiesType;
}) => {
  const [schema, setSchema] = useState(searchAliasJsonSchema);
  const { isCluster } = useEditViewContext();
  useEffect(() => {
    const nameProperty =
      isCluster && schema.properties
        ? {
            name: {
              ...schema.properties.name,
              const: view.name
            }
          }
        : {};
    const newProperties = schema.properties
      ? {
          ...schema.properties,
          id: {
            ...schema.properties.id,
            const: view.id
          },
          globallyUniqueId: {
            ...schema.properties.globallyUniqueId,
            const: view.globallyUniqueId
          },
          ...nameProperty
        }
      : undefined;
    const newSchema = {
      ...schema,
      // all updates need to update the ID
      $id: `${schema.$id}/${view.name}/1`,
      properties: newProperties
    };
    setSchema(newSchema);
    // Only run this once on mount, set const values
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  return { schema };
};
