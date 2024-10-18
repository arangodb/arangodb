import { JSONSchemaType } from "ajv";
import { useEffect, useState } from "react";
import { useEditViewContext } from "../editView/EditViewContext";
import { ArangoSearchViewPropertiesType } from "../View.types";

const extendedNames = window.frontendConfig.extendedNames;

export const arangoSearchViewJSONSchema: JSONSchemaType<ArangoSearchViewPropertiesType> =
  {
    $id: "https://arangodb.com/schemas/views/views.json",
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
        type: "string",
        pattern: extendedNames ? "" : "^[a-zA-Z][a-zA-Z0-9-_]*$"
      },
      type: {
        type: "string",
        const: "arangosearch"
      },
      links: {
        nullable: true,
        type: "object",
        patternProperties: {
          "^[a-zA-Z0-9-_]+$": {
            type: "object",
            nullable: true,
            $id: "https://arangodb.com/schemas/views/linkProperties.json",
            properties: {
              analyzers: {
                type: "array",
                nullable: true,
                items: {
                  type: "string",
                  pattern: "^([a-zA-Z0-9-_]+::)?[a-zA-Z][a-zA-Z0-9-_]*$"
                }
              },
              fields: {
                type: "object",
                nullable: true,
                required: [],
                patternProperties: {
                  "^[a-zA-Z0-9-_]+$": {
                    type: "object",
                    nullable: true,
                    $ref: "linkProperties.json"
                  }
                }
              },
              nested: {
                type: "object",
                nullable: true,
                required: [],
                patternProperties: {
                  "^[a-zA-Z0-9-_]+$": {
                    type: "object",
                    nullable: true,
                    $ref: "linkProperties.json"
                  }
                }
              },
              includeAllFields: {
                type: "boolean",
                nullable: true
              },
              trackListPositions: {
                type: "boolean",
                nullable: true
              },
              storeValues: {
                type: "string",
                nullable: true,
                enum: ["none", "id"]
              },
              inBackground: {
                type: "boolean",
                nullable: true
              },
              cache: {
                type: "boolean",
                nullable: true
              }
            },
            additionalProperties: false
          }
        },
        required: []
      },
      primarySort: {
        type: "array",
        nullable: true,
        items: {
          type: "object",
          nullable: false,
          properties: {
            field: {
              type: "string",
              nullable: false
            },
            asc: {
              type: "boolean",
              nullable: false
            }
          },
          default: {
            field: "",
            direction: "asc"
          },
          required: ["field", "asc"],
          additionalProperties: false
        }
      },
      primarySortCompression: {
        type: "string",
        nullable: true,
        enum: ["lz4", "none"],
        default: "lz4"
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
                type: "string"
              }
            },
            compression: {
              type: "string",
              enum: ["lz4", "none"],
              default: "lz4"
            },
            cache: {
              type: "boolean",
              nullable: true
            }
          },
          required: ["fields", "compression"],
          default: {
            compression: "lz4"
          },
          additionalProperties: false
        }
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
      },
      consolidationPolicy: {
        type: "object",
        nullable: true,
        discriminator: {
          propertyName: "type"
        },
        oneOf: [
          {
            properties: {
              type: {
                const: "bytes_accum"
              },
              threshold: {
                type: "number",
                nullable: false,
                minimum: 0,
                maximum: 1,
                default: 0.1
              }
            },
            additionalProperties: false
          },
          {
            properties: {
              type: {
                const: "tier"
              },
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
            additionalProperties: false
          }
        ],
        default: {
          type: "tier",
          segmentsMin: 1,
          segmentsMax: 10,
          segmentsBytesMax: 5368709120,
          segmentsBytesFloor: 2097152
        },
        required: ["type"]
      },
      primaryKeyCache: {
        type: "boolean",
        nullable: true
      },
      primarySortCache: {
        type: "boolean",
        nullable: true
      }
    },
    required: ["id", "name", "type"],
    additionalProperties: true
  };

export const useArangoSearchJSONSchema = ({
  view
}: {
  view: ArangoSearchViewPropertiesType;
}) => {
  const [schema, setSchema] = useState(arangoSearchViewJSONSchema);
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
