import { JSONSchemaType } from "ajv";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";
/**
 * 
  type: string;
  name?: string;
  inBackground: boolean;
  analyzer?: string;
  features: AnalyzerFeatures[];
  includeAllFields: boolean;
  trackListPositions: boolean;
  searchField: boolean;
  fields?: FieldType[] | null;
 */

const invertedIndexJSONSchema: JSONSchemaType<InvertedIndexValuesType> = {
  $id: "https://arangodb.com/schemas/views/invertedIndex.json",
  type: "object",
  properties: {
    type: {
      nullable: false,
      type: "string",
      const: "inverted"
    },
    name: {
      nullable: true,
      type: "string"
    },
    analyzer: {
      nullable: true,
      type: "string"
    },
    features: {
      nullable: true,
      type: "array",
      items: {
        type: "string"
      }
    },
    includeAllFields: {
      nullable: true,
      type: "boolean"
    },
    trackListPositions: {
      nullable: true,
      type: "boolean"
    },
    searchField: {
      nullable: true,
      type: "boolean"
    },
    inBackground: {
      nullable: true,
      type: "boolean"
    },
    fields: {
      $id: "https://arangodb.com/schemas/views/invertedIndexFields.json",
      type: "array",
      nullable: true,
      items: {
        type: "object",
        nullable: false,
        properties: {
          name: {
            type: "string",
            nullable: false
          },
          analyzer: {
            type: "string",
            nullable: true
          },
          features: {
            type: "array",
            nullable: true,
            items: {
              type: "string",
              nullable: true
            }
          },
          searchField: {
            type: "boolean",
            nullable: true
          },
          includeAllFields: {
            type: "boolean",
            nullable: true
          },
          trackListPositions: {
            type: "boolean",
            nullable: true
          },
          nested: {
            type: "array",
            $ref: "invertedIndexFields.json",
            nullable: true
          }
        },
        required: ["name"],
        additionalProperties: false
      }
    },
    primarySort: {
      type: "object",
      nullable: true,
      properties: {
        fields: {
          type: "array",
          nullable: false,
          items: {
            type: "object",
            nullable: false,
            properties: {
              field: {
                type: "string",
                nullable: false
              },
              direction: {
                type: "string",
                nullable: false
              }
            },
            default: {
              field: "",
              direction: "asc"
            },
            required: ["field", "direction"],
            additionalProperties: false
          }
        },
        compression: {
          type: "string",
          enum: ["lz4", "none"],
          default: "lz4"
        }
      },
      required: ["compression", "fields"]
    }
  },
  required: ["type"],
  additionalProperties: false
};

/**
 * Currently returns a static schema, to be modified in the future for update case
 */
export const useInvertedIndexJSONSchema = () => {
  return { schema: invertedIndexJSONSchema };
};
