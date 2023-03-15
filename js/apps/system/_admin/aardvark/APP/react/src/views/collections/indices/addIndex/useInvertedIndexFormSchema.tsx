import { JSONSchemaType } from "ajv";
import { InvertedIndexRequestType } from "./useCreateInvertedIndex";
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
const invertedIndexJSONSchema: JSONSchemaType<InvertedIndexRequestType> = {
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
      nullable: false,
      type: "array",
      items: {
        type: "string"
      }
    },
    includeAllFields: {
      nullable: false,
      type: "boolean"
    },
    trackListPositions: {
      nullable: false,
      type: "boolean"
    },
    searchField: {
      nullable: false,
      type: "boolean"
    },
    inBackground: {
      nullable: false,
      type: "boolean"
    },
    fields: {
      type: "array",
      nullable: true,
      items: {
        type: "string",
        nullable: false
      }
    }
  },
  required: ["type"],
  additionalProperties: false
};

export const useInvertedIndexFormSchema = () => {
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
