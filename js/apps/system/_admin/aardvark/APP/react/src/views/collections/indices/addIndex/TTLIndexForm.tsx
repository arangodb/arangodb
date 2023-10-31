import React from "react";
import { IndexFormInner } from "./IndexFormInner";
import {
  FIELDS,
  INITIAL_VALUES,
  SCHEMA,
  useCreateTTLIndex
} from "./useCreateTTLIndex";

export const TTLIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate } = useCreateTTLIndex();

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
