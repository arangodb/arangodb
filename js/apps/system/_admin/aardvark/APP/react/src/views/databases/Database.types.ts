export type DatabaseDescription = {
  name: string;
  isOneShard?: boolean;
  isSatellite?: boolean;
  replicationFactor?: number;
  writeConcern?: number;
};

export type DatabaseExtraDetails = {
  id: string;
  path: string;
  isSystem: boolean;
};
