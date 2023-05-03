import { Box } from "@chakra-ui/react";
import { chain, map } from "lodash";
import React, { useContext, useEffect, useState } from "react";
import { components, MultiValueGenericProps } from "react-select";
import useSWR from "swr";
import MultiSelect from "../../../../components/select/MultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { escapeFieldDot } from "../../../../utils/fieldHelpers";
import { FormState, ViewContext } from "../../constants";
import { useLinksContext } from "../../LinksContext";

const MultiValueLabel = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentField } = useLinksContext();
  return (
    <Box
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      cursor="pointer"
      onClick={() => {
        setCurrentField({ fieldPath: `links[${props.data.value}]` });
      }}
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};

const CollectionsDropdown = () => {
  const viewContext = useContext(ViewContext);
  const { setCurrentField } = useLinksContext();
  const { dispatch, formState: fs, isAdminUser } = viewContext;
  const formState = fs as FormState;
  const { data } = useSWR("/collection?excludeSystem=true", () =>
    getApiRouteForCurrentDB().get("/collection", "excludeSystem=true")
  );
  const [options, setOptions] = useState<{ label: string; value: string }[]>(
    []
  );
  useEffect(() => {
    if (data) {
      const collNames = map(data.body.result, "name");
      const tempOptions = collNames.sort().map(name => {
        return {
          label: name,
          value: name
        };
      });
      setOptions(tempOptions);
    }
  }, [data, formState.links]);

  const addLink = (link: string) => {
    const newLink = escapeFieldDot(link);
    dispatch({
      type: "setField",
      field: {
        path: `links[${newLink}]`,
        value: {}
      }
    });
  };

  const removeLink = async (link: string) => {
    const newLink = escapeFieldDot(link);
    dispatch({
      type: "setField",
      field: {
        path: `links[${newLink}]`,
        value: null
      }
    });
  };

  const validLinks = chain(formState.links)
    .toPairs()
    .filter(pair => pair[1] !== null)
    .map(pair => ({
      label: pair[0],
      value: pair[0]
    }))
    .value();
  return (
    <MultiSelect
      value={validLinks}
      options={options}
      placeholder="Enter a collection name"
      noOptionsMessage={() => "No collections found"}
      components={{
        MultiValueLabel
      }}
      isClearable={false}
      isDisabled={!isAdminUser}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeLink(action.removedValue.value);
          setCurrentField(undefined);
          return;
        }
        if (action.action === "select-option" && action.option?.value) {
          addLink(action.option.value);
        }
      }}
    />
  );
};

export default CollectionsDropdown;
