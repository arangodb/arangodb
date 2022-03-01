import React from "react";
import styled, { css } from "styled-components";

interface BadgeInfo {
  name: string | any;
}

type BadgeProps = {
  primary?: boolean;
  secondary?: boolean;
  tertiary?: boolean;
};

const BadgeContainer = styled.div<BadgeProps>`
  padding: 0.4rem 0.5rem;
  border-radius: 10px;
  font-size: 0.8rem;
  margin-left: 0.5rem;
  width: 50px;
  display: inline-block;
  &:hover {
    cursor: pointer;
  }
  ${props =>
    props.primary &&
    css`
      background-color: #40916c;
      color: #ffffff;
    `};
  ${props =>
    props.secondary &&
    css`
      background-color: #d3d3d3;
      color: #ffffff;
    `};
`;

const BadgeWrapper = styled.div`
  width: 100%;
`;

const Badge: React.FC<BadgeInfo> = ({ name }) => {
  return (
    <BadgeContainer primary={true}>
      <BadgeWrapper>{name}</BadgeWrapper>
    </BadgeContainer>
  );
};

export default Badge;
