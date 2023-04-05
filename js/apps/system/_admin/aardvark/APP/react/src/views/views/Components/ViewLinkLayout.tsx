import { Box, HStack, Link, Text } from "@chakra-ui/react";
import React, { ReactNode } from "react";
import { Link as RouterLink } from "react-router-dom";

type ViewLinkLayoutProps = {
  fragments?: string[];
  children: ReactNode;
};

const ViewLinkLayout = ({ fragments = [], children }: ViewLinkLayoutProps) => {
  return (
    <Box fontSize="md">
      <Box backgroundColor="gray.800" paddingX="4" paddingY="2">
        <HStack listStyleType="none" as="ul" color="gray.100" flexWrap="wrap">
          <Link
            textDecoration="underline"
            _hover={{ color: "gray.100" }}
            as={RouterLink}
            to={"/"}
          >
            Links
          </Link>
          <Box>{">>"}</Box>
          {fragments.slice(0, fragments.length - 1).map((fragment, idx) => {
            const path = fragments.slice(0, idx + 1).join("/");
            return (
              <HStack as="li" key={`${idx}-${fragment}`}>
                <Link
                  textDecoration="underline"
                  _hover={{ color: "gray.100" }}
                  as={RouterLink}
                  isTruncated
                  maxWidth="200px"
                  to={`/${path}`}
                  title={fragment}
                >
                  {fragment}
                </Link>
                <Box>{">>"}</Box>
              </HStack>
            );
          })}
          <Text
            isTruncated
            maxWidth="200px"
            title={fragments[fragments.length - 1]}
          >
            {fragments[fragments.length - 1]}
          </Text>
        </HStack>
      </Box>
      {children}
    </Box>
  );
};

export default ViewLinkLayout;
