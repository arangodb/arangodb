import { ArrowBackIcon } from "@chakra-ui/icons";
import { Box, Button, Spinner, Stack, Text } from "@chakra-ui/react";
import { omit } from "lodash";
import React, { useEffect, useState } from "react";
import useSWR from "swr";
import useSWRImmutable from "swr/immutable";
import { OptionType } from "../../../components/select/SelectBase";
import SingleSelect from "../../../components/select/SingleSelect";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { SearchAliasViewPropertiesType } from "../searchView.types";
import { SearchViewType } from "../viewsList/useViewsList";
import { useSearchAliasContext } from "./SearchAliasContext";

export const CopyPropertiesDropdown = () => {
  const { data, isLoading: isLoadingList } = useSWR("/view", path =>
    getApiRouteForCurrentDB().get(path)
  );

  const views = data?.body.result as SearchViewType[] | undefined;
  if (!views) {
    return null;
  }
  if (isLoadingList) {
    return <Spinner />;
  }
  return <CopyPropertiesInner views={views} />;
};

const CopyPropertiesInner = ({ views }: { views: SearchViewType[] }) => {
  const initialViewName = views?.find(
    view => view.type === "search-alias"
  )?.name;
  const [options, setOptions] = useState<OptionType[]>([]);
  const [selectedViewName, setSelectedViewName] = useState(initialViewName);
  const { encoded } = encodeHelper(selectedViewName || "");
  const { data: fullViewData, isLoading } = useSWRImmutable(
    `/view/${encoded}/properties`,
    path => getApiRouteForCurrentDB().get(path)
  );
  const selectedView = omit(
    fullViewData?.body,
    "error",
    "code"
  ) as SearchAliasViewPropertiesType;
  const { onCopy, isAdminUser } = useSearchAliasContext();
  useEffect(() => {
    if (views) {
      const newViews = views
        .filter(view => {
          return view.type !== "arangosearch";
        })
        .map(view => {
          return { label: view.name, value: view.name };
        });
      setOptions(newViews);
    }
  }, [views]);

  return (
    <Stack direction="row" alignItems="center">
      <Text>Copy mutable properties</Text>
      <Box width="44">
        <SingleSelect
          options={options}
          value={options.find(option => option.value === selectedViewName)}
          onChange={value => {
            setSelectedViewName(value?.value);
          }}
        />
      </Box>
      <Button
        size="xs"
        leftIcon={<ArrowBackIcon />}
        isLoading={isLoading}
        onClick={() =>
          onCopy({
            selectedView
          })
        }
        isDisabled={!isAdminUser}
      >
        Copy from
      </Button>
    </Stack>
  );
};
