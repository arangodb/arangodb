import { Box, FormLabel } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React, { useEffect, useState } from "react";
import useSWR from "swr";
import { SelectControl } from "../../../../components/form/SelectControl";
import { OptionType } from "../../../../components/select/SelectBase";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { IndexFormFieldProps } from "./IndexFormField";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

export const InvertedIndexAnalyzerDropdown = ({
  field,
  autoFocus
}: {
  field: IndexFormFieldProps;
  autoFocus: boolean;
}) => {
  const [options, setOptions] = useState<OptionType[]>([]);
  const { data: analyzersResponse } = useSWR("/analyzer", path =>
    getApiRouteForCurrentDB().get(path)
  );
  const analyzersList = analyzersResponse?.body.result as {
    name: string;
    features: string[];
  }[];
  const { values, touched, setFieldValue } = useFormikContext<
    InvertedIndexValuesType
  >();

  React.useEffect(() => {
    // set the value of textC, based on textA and textB
    if (values.analyzer) {
      const features = analyzersList.find(
        analyzer => analyzer.name === values.analyzer
      )?.features;
      console.log("setting value", { features, analyzersList });
      setFieldValue("features", features);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [analyzersList, touched.analyzer, values.analyzer]);

  useEffect(() => {
    if (analyzersList) {
      const tempOptions = analyzersList.map(option => {
        return {
          value: option.name,
          label: option.name
        };
      });
      setOptions(tempOptions);
    }
  }, [analyzersList]);

  return (
    <>
      <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
      <SelectControl
        isDisabled={field.isDisabled}
        selectProps={{
          autoFocus,
          options: options,
          isClearable: true
        }}
        isRequired={field.isRequired}
        name={field.name}
      />
      <Box></Box>
    </>
  );
};
