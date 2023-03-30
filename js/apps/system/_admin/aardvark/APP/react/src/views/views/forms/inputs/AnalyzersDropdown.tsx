import { get, without } from "lodash";
import React, { useContext, useEffect, useMemo, useState } from "react";
import useSWR from "swr";
import MultiSelect from "../../../../components/select/MultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { LinkProperties, ViewContext } from "../../constants";

export const AnalyzersDropdown = ({
  basePath,
  isDisabled,
  formState
}: {
  basePath: string;
  isDisabled: boolean;
  formState: LinkProperties;
}) => {
  const viewContext = useContext(ViewContext);
  const { dispatch } = viewContext;
  const { data } = useSWR("/analyzer", path =>
    getApiRouteForCurrentDB().get(path)
  );
  const [options, setOptions] = useState<OptionType[]>([]);
  const analyzers = useMemo(() => get(formState, "analyzers", [] as string[]), [
    formState
  ]);
  useEffect(() => {
    if (data) {
      const tempOptions = (data.body.result as { name: string }[]).map(
        option => {
          return {
            value: option.name,
            label: option.name
          };
        }
      );

      setOptions(tempOptions);
    }
  }, [data]);

  const addAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: "analyzers",
        value: analyzers.concat([analyzer as string])
      },
      basePath
    });
  };

  const removeAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: "analyzers",
        value: without(analyzers, analyzer)
      },
      basePath
    });
  };
  const newAnalyzers = analyzers.map(analyzer => {
    return { value: analyzer, label: analyzer };
  });
  return (
    <MultiSelect
      isClearable={false}
      value={newAnalyzers}
      options={options}
      noOptionsMessage={() => "Analyzer does not exist."}
      placeholder={"Start typing for suggestions."}
      isDisabled={isDisabled}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeAnalyzer(action.removedValue.value);
          return;
        }
        if (action.action === "select-option" && action.option) {
          addAnalyzer(action.option.value);
        }
      }}
    />
  );
};

export default AnalyzersDropdown;
