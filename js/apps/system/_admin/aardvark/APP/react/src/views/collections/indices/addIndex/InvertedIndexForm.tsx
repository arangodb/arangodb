import { Box, FormLabel, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React, { useEffect, useState } from "react";
import SplitPane from "react-split-pane";
import useSWR from "swr";
import { SelectControl } from "../../../../components/form/SelectControl";
import { OptionType } from "../../../../components/select/SelectBase";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import useElementSize from "../../../views/useElementSize";
import { IndexFormFieldProps } from "./IndexFormField";
import { FormActions, IndexFormFieldsList } from "./IndexFormFieldList";
import { InvertedIndexFormJSONEditor } from "./InvertedIndexFormJSONEditor";
import { useCreateInvertedIndex } from "./useCreateInvertedIndex";

export const InvertedIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate, initialValues, schema, fields } = useCreateInvertedIndex();
  const [sectionRef, sectionSize] = useElementSize();
  const sectionWidth = sectionSize.width;
  const maxSize = sectionWidth - 200;
  const localStorageSplitPos = localStorage.getItem("splitPos") || "400";
  let splitPos = parseInt(localStorageSplitPos, 10);
  if (splitPos > sectionWidth - 200) {
    splitPos = sectionWidth - 200;
  }
  return (
    <Formik
      onSubmit={async values => {
        await onCreate({ values });
      }}
      initialValues={initialValues}
      validationSchema={schema}
      isInitialValid={false}
    >
      {() => (
        <Form>
          <Stack spacing="4" ref={sectionRef}>
            <Stack direction="row" height="100vh">
              <SplitPane
                paneStyle={{ overflow: "auto" }}
                maxSize={maxSize}
                defaultSize={splitPos}
                onChange={size =>
                  localStorage.setItem("splitPos", size.toString())
                }
              >
                <IndexFormFieldsList
                  fields={fields}
                  renderField={({ field, autoFocus }) => {
                    if (field.name === "analyzer") {
                      return (
                        <AnalyzerField field={field} autoFocus={autoFocus} />
                      );
                    }
                    return <>{field.name}</>;
                  }}
                />
                <InvertedIndexFormJSONEditor />
              </SplitPane>
            </Stack>
            <FormActions onClose={onClose} />
          </Stack>
        </Form>
      )}
    </Formik>
  );
};

const AnalyzerField = ({
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
  const analyzersList = analyzersResponse?.body.result as { name: string }[];
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
