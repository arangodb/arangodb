import React from "react";
import { IndexFormInner } from "./IndexFormInner";
import {
  FIELDS,
  INITIAL_VALUES,
  SCHEMA,
  useCreateZKDIndex
} from "./useCreateZKDIndex";

export const ZKDIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate } = useCreateZKDIndex();

  return (
    <IndexFormInner<typeof INITIAL_VALUES, typeof FIELDS, typeof SCHEMA>
      initialValues={INITIAL_VALUES}
      schema={SCHEMA}
      fields={FIELDS}
      onCreate={onCreate}
      onClose={onClose}
    />
  );
};
