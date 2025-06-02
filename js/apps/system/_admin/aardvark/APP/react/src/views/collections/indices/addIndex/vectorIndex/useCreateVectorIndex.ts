import * as Yup from "yup";
import { commonFieldsMap, commonSchema } from "../IndexFieldsHelper";
import { useCreateIndex } from "../useCreateIndex";

export const INITIAL_VALUES = {
  type: "vector",
  fields: commonFieldsMap.fields.initialValue,
  name: commonFieldsMap.name.initialValue,
  params: {
    metric: "",
    dimension: undefined,
    nLists: undefined,
    defaultNProbe: 1,
    trainingIterations: 25,
  }
};

export const FIELDS = [
  commonFieldsMap.fields,
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
    label: "Number of Lists (nlists)",
    name: "params.nLists",
    type: "number",
    isRequired: true,
    tooltip: "The number of centroids (clusters) to divide the vector space into. A higher value improves recall but increases indexing time. Suggested value: ~15 × N, where N is the number of documents."
  },
  {
    label: "Default N Probe",
    name: "params.defaultNProbe",
    type: "number",
    tooltip: "The number of inverted lists (clusters) to search during queries by default. Increasing this value improves recall at the cost of speed. Default is 1."
  },
  {
    label: "Training Iterations",
    name: "params.trainingIterations",
    type: "number",
    tooltip: "The number of iterations to use during index training. More iterations improve cluster quality and accuracy, but increase training time. Default is 25."
  },
];

export const SCHEMA = Yup.object({
  ...commonSchema,
  params: Yup.object({
    metric: Yup.string().required("Metric is required"),
    dimension: Yup.number().required("Dimension is required"),
    nLists: Yup.number().required("Number of lists (nlists) is required"),
    defaultNProbe: Yup.number().optional(),
    trainingIterations: Yup.number().optional(),
  }),
});

type ValuesType = Omit<typeof INITIAL_VALUES, "fields"> & {
  fields: string[];
};

export const useCreateVectorIndex = () => {
  const { onCreate: onCreateIndex } = useCreateIndex<ValuesType>();
  const onCreate = async ({ values }: { values: typeof INITIAL_VALUES }) => {
    return onCreateIndex({
      type: values.type,
      fields: values.fields.split(",").map(field => field.trim()),
      name: values.name,
      params: values.params,
    });
  };
  return { onCreate };
};
