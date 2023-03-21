import React from "react";
import { IndexType } from "../../useFetchIndices";
import { InvertedIndexForm } from "./InvertedIndexFormWrap";
import {
  InvertedIndexValuesType,
  useInvertedIndexFieldsData
} from "./useCreateInvertedIndex";

export const InvertedIndexView = ({
  onClose,
  indexRow
}: {
  onClose: () => void;
  indexRow: IndexType;
}) => {
  const { schema, fields } = useInvertedIndexFieldsData();
  return (
    <InvertedIndexForm
      isFormDisabled
      initialValues={(indexRow as any) as InvertedIndexValuesType}
      schema={schema}
      fields={fields}
      onClose={onClose}
    />
  );
};
