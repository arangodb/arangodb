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
      type: "string"
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
    }
  },
  required: ["type"],
  additionalProperties: false
};

/**
 * 
type FieldType = {
  name: string;
  analyzer?: string;
  searchField?: boolean;
  features?: AnalyzerFeatures[];
  includeAllFields?: boolean;
  trackListPositions?: boolean;
  nested?: Omit<FieldType, "includeAllFields" | "trackListPositions">[];
};

 */
export const useInvertedIndexJSONSchema = () => {
  // const [schema, setSchema] = useState(invertedIndexJSONSchema);
  // useEffect(() => {
  //   const newProperties = schema.properties
  //     ? {
  //         ...schema.properties,
  //         id: {
  //           ...schema.properties.id,
  //           const: view.id
  //         },
  //         globallyUniqueId: {
  //           ...schema.properties.globallyUniqueId,
  //           const: view.globallyUniqueId
  //         },
  //         ...nameProperty
  //       }
  //     : undefined;
  //   const newSchema = {
  //     ...schema,
  //     // all updates need to update the ID
  //     $id: `${schema.$id}/${view.name}/1`,
  //     properties: newProperties
  //   };
  //   setSchema(newSchema);
  //   // Only run this once on mount, set const values
  //   // eslint-disable-next-line react-hooks/exhaustive-deps
  // }, []);
  return { schema: invertedIndexJSONSchema };
};
