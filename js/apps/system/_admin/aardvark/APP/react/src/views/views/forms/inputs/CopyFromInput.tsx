import { ArrowBackIcon } from "@chakra-ui/icons";
import { Box, Button, Stack, Text } from "@chakra-ui/react";
import { find, pick, sortBy } from "lodash";
import React, { useEffect, useState } from "react";
import useSWRImmutable from "swr/immutable";
import SingleSelect from "../../../../components/select/SingleSelect";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { FormProps } from "../../../../utils/constants";
import { encodeHelper } from "../../../../utils/encodeHelper";
import { FormState } from "../../constants";
import { validateAndFix } from "../../helpers";
import { useLinksContext } from "../../LinksContext";

type CopyFromInputProps = {
  views: FormState[];
} & Pick<FormProps<FormState>, "dispatch" | "formState">;

const filterAndSortViews = (views: FormState[]) => {
  return sortBy(
    views
      .filter(view => {
        return view.type === "arangosearch";
      })
      .map(view => {
        return { value: view.name, label: view.name };
      }),
    "name"
  ) as { value: string; label: string }[];
};

const CopyFromInput = ({ views, dispatch, formState }: CopyFromInputProps) => {
  const initalViewOptions = filterAndSortViews(views);
  const [viewOptions, setViewOptions] = useState(initalViewOptions);
  const [selectedView, setSelectedView] = useState(viewOptions[0]);
  const { encoded: encodedSelectedViewName } = encodeHelper(selectedView.value);
  const { data } = useSWRImmutable(
    `/view/${encodedSelectedViewName}/properties`,
    path => getApiRouteForCurrentDB().get(path)
  );
  const viewToCopy = data ? data.body : selectedView;
  const { setCurrentField } = useLinksContext();

  useEffect(() => {
    setViewOptions(filterAndSortViews(views));
  }, [views]);

  const copyFormState = () => {
    validateAndFix(viewToCopy);
    const nullifiedLinks = {} as any;
    if (formState.links) {
      Object.keys(formState.links).forEach(linkKey => {
        if (!viewToCopy.links[linkKey]) {
          // mark all the links to be deleted as null
          nullifiedLinks[linkKey] = null;
        }
      });
    }
    let newViewToCopy = {
      ...viewToCopy,
      links: { ...viewToCopy.links, ...nullifiedLinks }
    };
    const immutableProperties = pick(
      formState,
      "id",
      "name",
      "primarySort",
      "primarySortCompression",
      "storedValues",
      "writebufferIdle",
      "writebufferActive",
      "writebufferSizeMax"
    );
    newViewToCopy = { ...newViewToCopy, ...immutableProperties };
    dispatch({
      type: "setFormState",
      formState: newViewToCopy as FormState
    });
    dispatch({ type: "regenRenderKey" });
    setCurrentField(undefined);
  };

  const updateSelectedView = (value: string) => {
    const tempSelectedView = find(viewOptions, { value }) || viewOptions[0];

    setSelectedView(tempSelectedView);
  };

  return (
    <Stack direction="row" alignItems="center" flexWrap="wrap">
      <Text>Copy mutable properties</Text>
      <Stack direction="row" alignItems="center">
        <Box width="44">
          <SingleSelect
            options={viewOptions}
            value={viewOptions.find(
              option => option.value === selectedView.value
            )}
            onChange={value => {
              updateSelectedView(value?.value || viewOptions[0].value);
            }}
          />
        </Box>
        <Button size="xs" leftIcon={<ArrowBackIcon />} onClick={copyFormState}>
          Copy from
        </Button>
      </Stack>
    </Stack>
  );
};

export default CopyFromInput;
