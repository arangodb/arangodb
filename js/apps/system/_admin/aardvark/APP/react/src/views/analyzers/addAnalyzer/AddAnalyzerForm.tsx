import { Divider, Grid, Stack, Switch, Text } from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { SelectControl } from "../../../components/form/SelectControl";
import { TYPE_TO_LABEL_MAP } from "../AnalyzersHelpers";
import { AnalyzerTypes } from "../Analyzer.types";
import { AnalyzerTypeForm } from "./AnalyzerTypeForm";
import { useReinitializeForm } from "../useReinitializeForm";
import { useAnalyzersContext } from "../AnalyzersContext";

const ANALYZER_TYPE_OPTIONS = Object.keys(TYPE_TO_LABEL_MAP).map(type => {
  return {
    value: type,
    label: TYPE_TO_LABEL_MAP[type as AnalyzerTypes]
  };
});

export const AddAnalyzerForm = ({
  initialFocusRef
}: {
  initialFocusRef?: React.RefObject<HTMLInputElement>;
}) => {
  useReinitializeForm();
  const [analyzerTypeField] = useField<AnalyzerTypes>("type");
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <Grid templateColumns={"1fr 1fr"} gap="6">
      <Stack>
        <Text fontSize="lg">Basic</Text>
        <Divider />
        <Grid templateColumns={"1fr 1fr"} columnGap="4">
          <InputControl
            ref={initialFocusRef}
            isDisabled={isDisabled}
            name="name"
            label="Analyzer name"
          />
          <SelectControl
            isDisabled={isDisabled}
            name="type"
            label="Analyzer type"
            selectProps={{
              options: ANALYZER_TYPE_OPTIONS
            }}
          />
        </Grid>
      </Stack>
      <Stack>
        <Text fontSize="lg">Features</Text>
        <Divider />
        <Grid templateColumns={"1fr 1fr 1fr"} rowGap="4">
          <FeatureSwitch name="frequency" />
          <FeatureSwitch name="norm" />
          <FeatureSwitch name="position" />
        </Grid>
      </Stack>
      <AnalyzerTypeForm analyzerType={analyzerTypeField.value} />
    </Grid>
  );
};

const FEATURE_NAME_TO_LABEL_MAP = {
  frequency: "Frequency",
  norm: "Norm",
  position: "Position"
};

const FeatureSwitch = ({ name }: { name: string }) => {
  const [featuresField, , featuresFieldHelper] = useField("features");
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <Switch
      isDisabled={isDisabled}
      isChecked={featuresField.value.includes(name)}
      onChange={() => {
        if (featuresField.value.includes(name)) {
          featuresFieldHelper.setValue(
            featuresField.value.filter((feature: string) => feature !== name)
          );
          return;
        }
        featuresFieldHelper.setValue([...featuresField.value, name]);
      }}
    >
      {
        FEATURE_NAME_TO_LABEL_MAP[
          name as keyof typeof FEATURE_NAME_TO_LABEL_MAP
        ]
      }
    </Switch>
  );
};
