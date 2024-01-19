import { ExternalLinkIcon } from "@chakra-ui/icons";
import { Link, LinkProps, Stack, Text } from "@chakra-ui/react";
import React from "react";

export const ExternalLink = ({ href, children, ...rest }: LinkProps) => {
  return (
    <Link target="_blank" href={href} {...rest}>
      <Stack spacing="1" alignItems="center" direction="row">
        <Text>{children}</Text> <ExternalLinkIcon />
      </Stack>
    </Link>
  );
};
