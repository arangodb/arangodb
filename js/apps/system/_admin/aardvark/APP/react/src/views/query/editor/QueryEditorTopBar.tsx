import { Button, Flex, Icon, IconButton } from "@chakra-ui/react";
import React from "react";
import { MagicWand } from "styled-icons/boxicons-solid";
import SingleSelect from "../../../components/select/SingleSelect";
import { useQueryContext } from "../QueryContextProvider";
import { QuerySpotlight } from "./QuerySpotlight";

export const QueryEditorTopBar = () => {
  const {
    setCurrentView,
    onQueryChange,
    queryName,
    queryValue,
    queryBindParams,
    resetEditor,
    setResetEditor,
    setIsSpotlightOpen
  } = useQueryContext();
  const showNewButton =
    queryName !== "" ||
    queryValue !== "" ||
    JSON.stringify(queryBindParams) !== "{}";
  return (
    <Flex
      gap="2"
      direction="row"
      paddingY="2"
      borderBottom="1px solid"
      borderBottomColor="gray.300"
    >
      <Button
        size="sm"
        colorScheme="gray"
        onClick={() => {
          setCurrentView("saved");
        }}
      >
        Saved Queries
      </Button>
      {showNewButton && (
        <Button
          size="sm"
          colorScheme="gray"
          onClick={() => {
            onQueryChange({
              value: "",
              parameter: {},
              name: ""
            });
            setResetEditor(!resetEditor);
          }}
        >
          New
        </Button>
      )}
      <Flex marginLeft="auto" gap="2">
        <IconButton
          onClick={() => {
            setIsSpotlightOpen(true);
          }}
          icon={<Icon as={MagicWand} />}
          aria-label={"Spotlight"}
        />
        <QueryLimit />
      </Flex>
      <QuerySpotlight />
    </Flex>
  );
};

const QueryLimit = () => {
  const { setQueryLimit, queryLimit } = useQueryContext();
  const options = [
    { label: "100 results", value: "100" },
    { label: "250 results", value: "250" },
    { label: "500 results", value: "500" },
    { label: "1000 results", value: "1000" },
    { label: "2500 results", value: "2500" },
    { label: "5000 results", value: "5000" },
    { label: "10000 results", value: "10000" },
    { label: "All results", value: "all" }
  ];
  return (
    <SingleSelect
      value={options.find(option => {
        if (queryLimit === "all" && option.value === "all") {
          return option;
        }
        return Number(option.value) === Number(queryLimit);
      })}
      onChange={value => {
        if (!value) {
          return;
        }
        setQueryLimit(value.value === "all" ? "all" : Number(value.value));
      }}
      isClearable={false}
      options={options}
    />
  );
};
