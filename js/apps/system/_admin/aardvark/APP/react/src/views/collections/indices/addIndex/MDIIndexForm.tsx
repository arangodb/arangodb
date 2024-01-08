import React from "react";
import { IndexFormInner } from "./IndexFormInner";
import {
  FIELDS,
  INITIAL_VALUES,
  SCHEMA,
  useCreateMDIIndex
} from "./useCreateMDIIndex";

export const MDIIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate } = useCreateMDIIndex();

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
