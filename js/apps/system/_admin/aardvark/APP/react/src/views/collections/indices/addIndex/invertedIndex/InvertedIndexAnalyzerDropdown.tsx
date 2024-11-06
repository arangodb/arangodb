import { SingleSelectControl, OptionType } from "@arangodb/ui";
import { FormLabel, Spacer } from "@chakra-ui/react";
import { useField, useFormikContext } from "formik";
import React, { useEffect, useState } from "react";
import useSWR from "swr";
import { FormFieldProps } from "../../../../../components/form/FormField";
import { getCurrentDB } from "../../../../../utils/arangoClient";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

export const InvertedIndexAnalyzerDropdown = ({
  field,
  autoFocus,
  dependentFieldName = "features"
}: {
  field: FormFieldProps;
  autoFocus: boolean;
  dependentFieldName?: string;
}) => {
  const [options, setOptions] = useState<OptionType[]>([]);
  const { data: analyzersList } = useSWR("/analyzer", () =>
    getCurrentDB().listAnalyzers()
  );
  const { setFieldValue } = useFormikContext<InvertedIndexValuesType>();
  const [formikField] = useField(field.name);
  const analyzerName = formikField.value;
  React.useEffect(() => {
    if (field.isDisabled) {
      return;
    }
    if (analyzerName) {
      const features = analyzersList?.find(
        analyzer => analyzer.name === analyzerName
      )?.features;
      setFieldValue(dependentFieldName, features);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [analyzersList, analyzerName, dependentFieldName]);

  useEffect(() => {
    if (field.isDisabled) {
      setOptions([
        {
          value: analyzerName,
          label: analyzerName
        }
      ]);
      return;
    }
    if (analyzersList) {
      const tempOptions = analyzersList.map(option => {
        return {
          value: option.name,
          label: option.name
        };
      });
      setOptions(tempOptions);
    }
  }, [analyzerName, analyzersList, field.isDisabled]);
  return (
    <>
      <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
      <SingleSelectControl
        isDisabled={field.isDisabled}
        selectProps={{
          autoFocus,
          options: options,
          isClearable: true
        }}
        isRequired={field.isRequired}
        name={field.name}
      />
      <Spacer />
    </>
  );
};
