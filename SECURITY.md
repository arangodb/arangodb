# Security Policy

## Reporting a vulnerability

Two ways to reach us. Either is fine, the first is preferred.

1. **GitHub private reporting.** Use
   [Report a vulnerability](https://github.com/arangodb/arangodb/security/advisories/new)
   on this repository, or the Security tab of whichever repository is affected. This
   creates a private advisory only our security team can see, and it keeps the discussion
   attached to the code.
2. **Email [security@arango.ai](mailto:security@arango.ai).** Use this for anything that
   is not tied to one repository, or if you would rather not use GitHub.

Please do not open a public issue or pull request for a suspected vulnerability, and
please do not discuss it publicly until we have published an advisory.

Helpful things to include: affected version or commit, affected component, what an
attacker gains, and the smallest set of steps that shows the problem. A proof of concept
speeds triage considerably. English and German are both fine.

## What you can expect from us

| Stage | Target |
|---|---|
| Acknowledgement that a human has your report | 72 hours |
| Initial assessment, including whether we can reproduce it | 10 business days |
| Status update while the report is open | at least every 14 days |
| Coordinated disclosure | within 90 days of receipt, or when the fix ships, whichever comes first |

If a report is Critical we will move faster than the table and will tell you so.

We will keep you informed as we go, tell you plainly if we disagree that something is a
vulnerability and why, credit you in the published advisory unless you ask us not to, and
agree the disclosure timing with you before we publish. If we need longer than 90 days,
for example because a fix requires a coordinated release, we will explain why and agree a
new date with you rather than going quiet.

For High and Critical issues in the products listed below we will request a CVE, normally
through GitHub as CNA, and reference it in the published advisory.

## Scope

In scope, all supported versions:

- **ArangoDB server**, `arangodb/arangodb`, including arangod, arangosh, the web
  interface and Foxx.
- **Official drivers and clients**: `python-arango`, `python-arango-async`, `arangojs`,
  `arangodb-java-driver`, `go-driver`, `arangodb-php`, `arangodb-net-standard`.
- **Kubernetes operator and tooling**: `kube-arangodb`, `arangodb-helper` repositories
  we maintain.
- **ArangoDB Managed / Oasis** service and its client tooling, including `oasisctl`.
- **arango.ai and cloud.arangodb.com** web properties.

Out of scope:

- Third-party forks we host for build purposes. Several repositories in our
  organizations are forks of upstream projects such as RocksDB, curl, V8, cilium and
  fmt. Report issues in that code to the upstream project, which is the only place a fix
  can be made.
- End-of-life versions. See the
  [support and end-of-life policy](https://arango.ai/arangodb-product-support-end-of-life-announcements/).
- Findings with no security impact: missing hardening headers with no demonstrated
  attack, version disclosure on its own, results from automated scanners without a
  working proof of concept, and denial of service that requires privileges the attacker
  would already need for a worse outcome.
- Social engineering of our staff, physical attacks, and anything requiring a
  compromised end-user device.

## Safe harbour

We will not pursue or support legal action against you, and we will treat your research
as authorised, if you make a good-faith effort to follow this policy. Specifically:

- Test only against your own deployments or accounts, or against systems we have
  explicitly authorised in writing.
- Do not access, modify, exfiltrate or retain data that is not yours. If you encounter
  data belonging to someone else, stop, and tell us what you saw.
- Do not degrade our services or our customers' services. No load testing, no
  brute-force at volume, no attacks against availability.
- Give us a reasonable chance to fix the issue before you disclose it publicly, per the
  timings above.

If a third party takes action against you for research that followed this policy, we will
make it known that your work was authorised.

This is not permission to test systems operated by our customers, and it does not
override any other agreement you have with us.

## Supported versions

We release security patches for versions listed as supported in the
[support and end-of-life policy](https://arango.ai/arangodb-product-support-end-of-life-announcements/).

## Published advisories

Advisories are published on the affected repository's Security tab. Once a CVE is
assigned, they also appear in the GitHub Advisory Database.
