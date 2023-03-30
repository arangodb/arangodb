import { JSONSchemaType } from "ajv";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

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
    },
    storedValues: {
      type: "array",
      nullable: true,
      items: {
        type: "object",
        nullable: false,
        properties: {
          fields: {
            type: "array",
            nullable: false,
            items: {
              type: "string",
              nullable: false
            }
          },
          compression: {
            type: "string",
            nullable: false
          }
        },
        default: {
          field: "",
          compression: "asc"
        },
        required: ["fields", "compression"],
        additionalProperties: false
      }
    },
    consolidationPolicy: {
      type: "object",
      nullable: true,
      properties: {
        type: {
          const: "tier",
          type: "string"
        },
        threshold: { type: "integer", nullable: true },
        segmentsMin: {
          type: "integer",
          nullable: false,
          minimum: 0,
          maximum: {
            $data: "1/segmentsMax"
          },
          default: 1
        },
        segmentsMax: {
          type: "integer",
          nullable: false,
          minimum: {
            $data: "1/segmentsMin"
          },
          default: 10
        },
        segmentsBytesMax: {
          type: "integer",
          nullable: false,
          minimum: 0,
          default: 5368709120
        },
        segmentsBytesFloor: {
          type: "integer",
          nullable: false,
          minimum: 0,
          default: 2097152
        },
        minScore: {
          type: "number",
          nullable: false,
          minimum: 0,
          maximum: 1,
          default: 0
        }
      },
      required: ["type"],
      additionalProperties: false
    },
    cleanupIntervalStep: {
      type: "integer",
      nullable: true,
      minimum: 0,
      default: 2
    },
    commitIntervalMsec: {
      type: "integer",
      nullable: true,
      minimum: 0,
      default: 1000
    },
    consolidationIntervalMsec: {
      type: "integer",
      nullable: true,
      minimum: 0,
      default: 1000
    },
    writebufferIdle: {
      type: "integer",
      nullable: true,
      minimum: 0,
      default: 64
    },
    writebufferActive: {
      type: "integer",
      nullable: true,
      minimum: 0,
      default: 0
    },
    writebufferSizeMax: {
      type: "integer",
      nullable: true,
      minimum: 0,
      default: 33554432
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
