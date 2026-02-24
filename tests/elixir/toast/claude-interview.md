# Interview Transcript

## Q1: What is the scope of this planning exercise?

**Answer:** Extend and/or rebuild — keep what's good, replace/rebuild what's not good. Toast replaces Armadillo; Elixir is the chosen path.

## Q2: What specific pain points or shortcomings exist in the current Toast implementation?

**Answer:** The parts that are currently there work fine (at least based on the limited testing done so far). A few small things regarding reporting etc. could still be improved, but those are rather minor. What it is currently lacking:

- **Suite system**: Need a more sophisticated suite system like the JS system has, where suites can control/manage their own deployments.
- **Infrastructure as library**: Want Toast to provide flexible infrastructure to manage deployments usable in interactive sessions. The test framework should be one consumer of the infrastructure.
- **Resilience testing**: Tests need to work directly with the deployment — start/stop servers, pause servers (SIGSTOP), cause deliberate crashes. Monitoring should not be triggered during deliberate actions. The old JS framework has such tests but they are "very obscure" — there has to be a better way.
- **Umbrella structure**: The umbrella app was originally needed to overcome ExUnit module ordering limitations. Now that there's a custom mix task and runner, the umbrella feels like a poor solution. Should flatten to a single project with integration tests in a separate folder next to unit tests.
- **Missing features**: Analyze tool for detailed analysis from report JSON, and other features listed below.

## Q3: What are the highest-priority missing features?

**Answer:** More test suites, hot restart/resilience testing, coredump analysis, analyzer tool, test result package creation for CI, thin client implementation over ArangoDB REST API to simplify writing tests.

## Q4: What does an interactive session look like?

**Answer:** Both IEx REPL for exploration (start deployments, run ad-hoc queries, inspect state, debug interactively) AND scripted automation (Elixir scripts using deployment infrastructure outside of ExUnit).

## Q5: How should suites relate to deployments?

**Answer:** In the old JS system, suites are very high-level concepts with many tests. You always explicitly specify the suite to run; usually a single suite, but could be more. Can run in single server or cluster mode. For most suites this determines the deployment, but some (resilience, replication) have their own deployment logic with specific requirements. If you specify two suites, each gets its own deployment, but all tests within a single suite share the same deployment.

Whether this is the right approach or whether something better exists is an open question — one of the areas of most uncertainty.

## Q6: What scope should the REST client have?

**Answer:** Minimal + extensible. Start minimal, designed so test suites can extend with domain-specific calls.

## Q7: How should resilience testing handle health monitoring?

**Answer:** This is an open design question — undecided. Needs input and a proposed approach.

## Q8: Where should integration tests live (project structure)?

**Answer:** Current idea: single project with:
- `lib/` — Toast infrastructure implementation
- `test/` — Typical ExUnit unit tests for the infrastructure
- `suite/` (folder name open to suggestions) — Integration/system tests

This gives clear separation between test types. Would need to implement module search and loading (custom toast task already exists). Open question: how to structure everything INSIDE the suite folder.

## Q9: How should suites be organized within the suite folder?

**Answer:** Open question — all approaches (folder-based, tag-based, manifest-based, hybrid) have pros and cons. Also open to discussing whether the JS-style suite organization makes sense at all. The current JS framework should work as a blueprint for the *kind of tests* needed, but NOT for how to build a framework for those tests. Everything is up for discussion — the point is to identify what we need and pick the best approach.

## Q10: What level of coredump analysis is needed?

**Answer:** Same as JS framework — want stack traces in the results so they can be analyzed without downloading the dump. For cases where that's insufficient, the dump should be available as a CI artifact.

## Q11: What's the timeline?

**Answer:** Incremental over months. No hard deadline.

## Q12: Summary check — does this capture the vision?

**Answer:** Yes, it captures it well — however, the open questions mentioned during the interview remain:
- How to handle monitoring during deliberate resilience actions
- Whether JS-style suite organization makes sense
- How to structure the suite folder
- What the suite system should look like

## Q13: Any hard constraints?

**Answer:** No special constraints. Standard Elixir project, no unusual requirements.
