import React from "react";
import { IndexFormInner } from "./IndexFormInner";
import {
  FIELDS,
  INITIAL_VALUES,
  SCHEMA,
  useCreatePersistentIndex
} from "./useCreatePersistentIndex";

export const PersistentIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate } = useCreatePersistentIndex();

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
