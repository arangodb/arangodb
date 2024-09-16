import { Database } from "arangojs";

async function globalTeardown() {
  const db = new Database({
    url: "http://localhost:3001"
  });
  // purge the database
  db.dropDatabase("test");
  console.log("Database purged");
}

export default globalTeardown;
