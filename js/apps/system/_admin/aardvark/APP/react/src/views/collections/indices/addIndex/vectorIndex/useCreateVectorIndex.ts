import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "../IndexFieldsHelper";
import { useCreateIndex } from "../useCreateIndex";

export const INITIAL_VALUES = {
  type: "vector",
  fields: "",
  name: commonFieldsMap.name.initialValue,
  params: {
    metric: "",
    dimension: undefined,
    nLists: undefined,
    defaultNProbe: 1,
    trainingIterations: 25,
    factory: undefined
  },
  parallelism: 2,
  inBackground: commonFieldsMap.inBackground.initialValue
};

export const FIELDS = [
  {
    label: "Field",
    name: "fields",
    type: "text",
    isRequired: true,
    tooltip: "The name of the attribute that contains the vector embeddings. Only a single attribute is supported."
  },
  commonFieldsMap.name,
  {
    label: "Metric",
    name: "params.metric",
    type: "creatableSingleSelect",
    isRequired: true,
    options: [
      { label: "Cosine", value: "cosine" },
      { label: "L2 (Euclidean)", value: "l2" }
    ],
    tooltip: "Distance metric used for similarity search. Choose between cosine similarity or Euclidean distance."
  },
  {
    label: "Dimension",
    name: "params.dimension",
    type: "number",
    isRequired: true,
    tooltip: "The dimensionality of the vector embeddings. The indexed attribute must contain vectors of this length."
  },
  {
    label: "Number of Lists (nLists)",
    name: "params.nLists",
    type: "number",
    isRequired: true,
    tooltip: "The number of Voronoi cells (nLists) to partition the vector space into. A higher value improves recall but increases indexing time. The value must not exceed the number of documents. Suggested: 15 * sqrt(N), where N is the number of documents."
  },
  {
    label: "Default Number of Probes",
    name: "params.defaultNProbe",
    type: "number",
    tooltip: "The number of inverted lists (clusters) to search during queries by default. Increasing this value improves recall at the cost of speed. The default is 1."
  },
  {
    label: "Training Iterations",
    name: "params.trainingIterations",
    type: "number",
    tooltip: "The number of iterations to use during index training. More iterations improve cluster quality and accuracy, but increase training time. The default is 25."
  },
  {
    label: "Index Factory",
    name: "params.factory",
    type: "text",
    tooltip: `Defines the FAISS index factory. Must start with "IVF". Example: IVF100_HNSW10,Flat. The number following "IVF" must match nLists (e.g. IVF100 → nLists = 100).`
  },
  {
    label: "Parallelism",
    name: "parallelism",
    type: "number",
    tooltip: "Number of threads to use for indexing. The default is 2."
  },
  commonFieldsMap.inBackground,
];

export const SCHEMA = Yup.object({
  fields: Yup.string().trim().required("Field is required"),
  name: commonSchema.name,
  params: Yup.object({
    metric: Yup.string().required("Metric is required"),
    dimension: Yup.number().required("Dimension is required"),
    nLists: Yup.number().required("Number of lists (nLists) is required"),
    defaultNProbe: Yup.number().optional(),
    trainingIterations: Yup.number().optional(),
    factory: Yup.string().optional()
  }),
  parallelism: Yup.number().optional(),
  inBackground: commonSchema.inBackground
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields"> & {
  fields: string[];
};

export const useCreateVectorIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      type: values.type,
      fields: [values.fields.trim()],
      name: values.name,
      params: values.params,
      parallelism: values.parallelism,
      inBackground: values.inBackground
    });
  };
  return { onCreate };
};
