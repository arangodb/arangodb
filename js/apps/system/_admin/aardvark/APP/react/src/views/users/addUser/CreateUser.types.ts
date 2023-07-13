export type CreateDatabaseUserValues = {
  name: string;
  extra: {
    img: string;
    name: string;
  };
  active: boolean;
  user: string;
  gravatar: string;
  passwd: string;
};

export type DatabaseUserValues = {
  extra: {
    img: string;
    name: string;
  };
  user: string;
  active: boolean;
};
