import { test } from "@playwright/test";
import { Database } from "arangojs";

test("teardown", async () => {
  console.log("Tearing down");
  const db = new Database({
    url: "http://localhost:3001",
    auth: { username: "root", password: "" }
  });
  await db.dropDatabase("test");
  console.log("Database test dropped");
  console.log("Teardown complete");
});
