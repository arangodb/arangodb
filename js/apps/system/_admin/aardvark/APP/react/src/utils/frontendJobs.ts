import { getCurrentDB, getApiRouteForCurrentDB } from "./arangoClient";

/**
 * Frontend job tracking utility.
 *
 * Stores async job IDs in the _frontend collection to track long-running
 * operations (like view updates). Uses the arangojs SDK for collection access.
 */

export interface FrontendJob {
  _key?: string;
  id: string;           // async job ID from x-arango-async-id header
  type: string;         // job type, e.g. "view"
  collection: string;   // name of the resource being operated on
  desc: string;         // human-readable description
  model: string;        // always "job" for job entries
}

const FRONTEND_COLLECTION = "_frontend";

const getFrontendCollection = () => {
  return getCurrentDB().collection(FRONTEND_COLLECTION);
};

/**
 * Add a job entry to the _frontend collection.
 */
export const addFrontendJob = async (job: Omit<FrontendJob, "model" | "_key">): Promise<void> => {
  const collection = getFrontendCollection();
  try {
    await collection.save({
      ...job,
      model: "job"
    });
  } catch (error) {
    console.error("Failed to add frontend job:", error);
    throw error;
  }
};

/**
 * Get all job entries from the _frontend collection.
 */
export const getFrontendJobs = async (): Promise<FrontendJob[]> => {
  const db = getCurrentDB();
  try {
    const cursor = await db.query<FrontendJob>({
      query: `FOR doc IN @@collection FILTER doc.model == "job" RETURN doc`,
      bindVars: { "@collection": FRONTEND_COLLECTION }
    });
    return await cursor.all();
  } catch (error: any) {
    // Collection might not exist yet, return empty array
    if (error?.code === 404 || error?.response?.status === 404) {
      return [];
    }
    console.error("Failed to get frontend jobs:", error);
    throw error;
  }
};

/**
 * Delete a specific job entry by its async job ID.
 */
export const deleteFrontendJob = async (jobId: string): Promise<void> => {
  const db = getCurrentDB();
  try {
    await db.query({
      query: `FOR doc IN @@collection FILTER doc.model == "job" AND doc.id == @jobId REMOVE doc IN @@collection`,
      bindVars: { "@collection": FRONTEND_COLLECTION, jobId }
    });
  } catch (error: any) {
    // Ignore 404 - job may already be deleted
    if (error?.code === 404 || error?.response?.status === 404) {
      return;
    }
    console.error("Failed to delete frontend job:", error);
    throw error;
  }
};

/**
 * Delete all job entries from the _frontend collection.
 */
export const deleteAllFrontendJobs = async (): Promise<void> => {
  const db = getCurrentDB();
  try {
    await db.query({
      query: `FOR doc IN @@collection FILTER doc.model == "job" REMOVE doc IN @@collection`,
      bindVars: { "@collection": FRONTEND_COLLECTION }
    });
  } catch (error: any) {
    if (error?.code === 404 || error?.response?.status === 404) {
      return;
    }
    console.error("Failed to delete all frontend jobs:", error);
    throw error;
  }
};

/**
 * Get pending async job IDs from the server.
 */
export const getPendingAsyncJobs = async (): Promise<string[]> => {
  const route = getApiRouteForCurrentDB();
  try {
    const result = await route.get("/job/pending");
    return result.body || [];
  } catch (error) {
    console.error("Failed to get pending jobs:", error);
    return [];
  }
};

/**
 * Check if an async job is complete by calling PUT /job/:id.
 * Returns the job result if complete, or null if still pending.
 */
export const checkAsyncJobStatus = async (jobId: string): Promise<{ complete: boolean; result?: any; error?: any }> => {
  const route = getApiRouteForCurrentDB();
  try {
    const result = await route.put(`/job/${jobId}`);
    if (result.status === 204) {
      // Job still pending
      return { complete: false };
    }
    // Job complete
    return { complete: true, result: result.body };
  } catch (error: any) {
    const statusCode = error?.response?.status;
    if (statusCode === 404 || statusCode === 400) {
      // Job not found or invalid - treat as complete (already processed)
      return { complete: true, error };
    }
    throw error;
  }
};

/**
 * Sync frontend jobs with actual pending async jobs.
 * Returns jobs that are still pending, and cleans up completed ones.
 */
export const syncFrontendJobs = async (typeFilter?: string): Promise<FrontendJob[]> => {
  const [frontendJobs, pendingJobIds] = await Promise.all([
    getFrontendJobs(),
    getPendingAsyncJobs()
  ]);

  const pendingSet = new Set(pendingJobIds);
  const stillPending: FrontendJob[] = [];

  for (const job of frontendJobs) {
    // Filter by type if specified
    if (typeFilter && job.type !== typeFilter) {
      continue;
    }

    if (pendingSet.has(job.id)) {
      stillPending.push(job);
    } else {
      // Job no longer pending, clean it up
      await deleteFrontendJob(job.id);
    }
  }

  // If no jobs are pending but we had frontend jobs, clean them all up
  if (pendingJobIds.length === 0 && frontendJobs.length > 0) {
    await deleteAllFrontendJobs();
  }

  return stillPending;
};
