# SD-02 Security Requirements Review (prompt v2)

This is the prompt used to create and amend `docs/security-requirements.md`. It is kept
beside the record so the instructions that produced it are traceable. Improve the prompt
here, then re-run it; the record and the prompt evolve together by pull request.

## Inputs (fill in per repository before running)

| Field | Value |
|---|---|
| Repository | `WATTER-Technology-Solutions/linux-rpi` (public fork of `raspberrypi/linux`), branch `watter-rpi-6.15.y` |
| Jira ticket | `SEC-98` (subtask of SEC-93) |
| What the software is | WATTER's changes to the Raspberry Pi Linux kernel for the "apollo" board (Raspberry Pi 5 controller of the WATTER water heater): IIO/hwmon drivers (mcp9600, pcf8593-ctr, ad7998 and tmp116 device-tree entries), an IIO "status" info type and `IIO_VAL_EMPTY`, apollo and squid device-tree overlays, the `watter-configs/apollo` kernel config, the `watter-build` script and Debian packaging changes. In scope: the non-merge commits by `@watter.com` authors since merge-base `fc85704c` (Linux 6.15.2). Everything else is vendored upstream Linux and Raspberry Pi code. |
| Security zone | WATTER-Guard (the kernel runs on the appliance's Raspberry Pi); built on an engineer's host |
| Deployment profile | Residential by evidence (same controller as watter-collector); MEGAWATT unconfirmed |
| Runtime identity | the kernel; sysfs, device nodes and boot files are governed by root and by the image's udev rules (outside this repo). `watter-build` runs as the invoking engineer |
| Known facts outside the repo | From benmcollins, 2026-09-30, SEC-93 rollout (recorded on watter-router PR #1): (1) engineering tooling runs in single-user arm64 Ubuntu Docker containers on developer laptops, or in single-user VMs, and all contents are destroyed after use; (2) the Raspberry Pi controller software is recorded in watter-collector `docs/security-requirements.md`, cross-referenced rather than re-described. Related SEC-93 records: watter-setup PR #3 (APT trust anchor, WATTER archive keyring), watter-tailnet PR #2 (Tailscale ACL, `tag:wattertop` is the RPi). |
| Previous record | none (first review) |
| Prompt version | v2, 2026-09-01 |

## What to produce

`docs/security-requirements.md`, required by WATTER ISOP-11 Secure Development rule SD-02
(ISO/IEC 27001:2022 A.8.26). It states exactly three things and nothing else:

1. What data the software handles, and at what classification.
2. Who is permitted to access that data, and how that is enforced.
3. What events must be logged.

No threat model, checklist, compliance matrix, roadmap or policy. ISOP-11 SD-11 forbids a
separate register; this file is the record, and the PR approval is the approval record.

### Policy context

Classification (ISWI-08-01):
- **Public**: no restriction.
- **Internal**: default for business information, including software version and build
  metadata, and operational telemetry that cannot be tied to a customer or household.
- **Confidential**: customer data, credentials, source code, anything whose disclosure
  would harm WATTER or a customer. Requires encryption in transit. This includes any data
  tied to an identifiable customer device or site (device ids, usage patterns, earnings),
  and data about third parties processed on customer hardware (for example GPU renters'
  workloads).
- **CUI**: may exist only in WATTER-Guard-CUI. WATTER holds none today. If you believe
  the repo touches CUI, stop and say so instead of writing the file.

Zones: WATTER-Central (internal IT), WATTER-Guard (customer-facing neo-cloud; profiles
MEGAWATT rack-scale and Residential appliances in uncontrolled locations),
WATTER-Guard-CUI (provisioned, empty).

Logging (ISOP-11 SD-04 rule 5), as summarised for this review: log authentication success
and failure, privilege change and configuration change; never log secrets or sensitive
payloads. Paste the verbatim rule text here when available so the mapping table cites the
control rather than this summary.

Review (ISOP-11 SD-05, A.8.28): every pull request is reviewed before merge, by AI assisted
review, by the other developer, or both; the pull request is the record of review. The
record PR therefore needs a reviewer assigned and must not be merged by the agent.

## How to review

Read the code before writing. Work in three parallel passes, each producing file:line
evidence, then reconcile them. Do not modify project files during the review; read-only
probes (a throwaway script in a scratch directory) are allowed and encouraged when a
claim is high impact and cheap to test, for example whether a config write persists a
secret to disk.

**Scope.** "The software" is what the build artifact installs or deploys (packaging
manifests, Dockerfiles, IaC, install lists), plus scripts in the repo that operators run
against production. Mark anything in the repo that is not shipped (helper scripts for
other devices, vendored third-party tools) as such and review it more lightly. Say which
files are vendored.

**Pass 1, data.** For every kind of data: what it is, source, whether it **persists**
(path, file mode as set in code or packaging, retention from logrotate/journald/DB TTL)
or only **transits** (protocol, destination host, TLS or not), and every place it is sent
(topics, endpoints, sockets, logs). Look at models, schemas, migrations, request and
response shapes, config defaults and their descriptions, env var names, storage clients,
outbound calls, log sinks. Credentials, keys, tokens, certificates, customer identifiers,
usage data, financial data and third-party data all count. Check whether the config
system itself persists secrets it only meant to hold in memory.

**Pass 2, access.** For each channel in and out (cloud API, local listeners, peer
links, device control, provisioning, local filesystem, source repo): the real mechanism
as implemented. Name it: IdP, API key, mTLS, per-unit certificate, HTTP Digest, network
position, IAM policy, filesystem mode, group membership. For each listener record bind
address, port, auth, TLS. For each inbound command record what validation exists and what
the command can cause. If access rests on network position or radio proximity alone, say
so in those words. Check packaging for `User=`, sudoers, setuid, socket groups, file modes.

**Pass 3, logging.** Map the pipeline (sinks, retention, anything forwarded off-device).
For each SD-04 rule 5 event, cite the log line that records it or write "not logged". Then
hunt for secrets in logs: credentials in URLs that get logged, exception strings carrying
request URLs, debug dumps of payloads or config, provisioning scripts echoing key material,
`print` of received credentials. Give each finding a verdict: LOGGED SECRET, SAFE, or
UNCERTAIN.

## Rules for what you write

- State only what the code shows. If you cannot determine something, write
  `UNKNOWN — <specific question, addressed to a role: infra owner, product owner, IT>`.
  An unanswered question is useful; an invented answer is a false statement in a controlled
  record. Check the "Known facts" input before writing an UNKNOWN.
- Describe what is true today, not what should be true. Do not describe intended design
  as implemented. Gaps go in the PR description, not in the file, except that section 3
  shows required versus current logging side by side.
- Every non-obvious claim in the file must be traceable to a file, and the PR description
  must carry the file:line evidence for each gap.
- Prefer tables to prose. As short as the facts allow: roughly one page per deployable
  component. Do not pad and do not summarise away a listener or a credential to hit a
  length.

## File template (use these headings and columns exactly)

```
# Security Requirements: <repo>

Record required by ISOP-11 SD-02 (ISO/IEC 27001:2022 A.8.26). Created and amended by
pull request; the PR approval is the record.

Reviewed: commit <hash> (<version/tag>), <date>, prompt v2. Previous record: <hash|none>.
Scope: <one paragraph: what the software is, where it runs, as what identity, what in the
repo is unshipped or vendored>.
Zone: <zone>, profile <profile>. CUI: none.

## 1. Data handled and classification
| Data | Persists / transits | Where (path, mode, retention / protocol, destination) | Class |

## 2. Who may access the data, and how that is enforced
| Path | Mechanism in the code today |
(one row per channel; include Local users and Source code rows; UNKNOWN inline)

## 3. Events that must be logged
<two sentences: where logs go today and retention>
| Event | Required | Today |
(rows: Authentication success, Authentication failure, Privilege change, Configuration
change, then system-specific rows such as safety or power actions)
**Must not be logged:** <list>, followed by what currently violates it.
```

## Deliver

1. Branch `<ticket>` from the default branch. Commit messages start with `<ticket>: `.
2. Commit only `docs/security-requirements.md` and this prompt as
   `docs/security-requirements-prompt.md` with the Inputs table filled in. Do not fix any
   gap you find in this PR.
3. Open a PR against the default branch titled `<ticket>: Add docs/security-requirements.md
   (ISOP-11 SD-02)` (or `Amend` in amend mode). Assign it to the repository owner and
   request their review (SD-05). Do not merge it. PR body sections, in this order:
   - **Summary** (three lines) and the Jira link.
   - **UNKNOWN items needing a human answer**, numbered, each addressed to a role.
   - **Security gaps found while reading**, grouped as: logged secrets; credential storage
     and transport; unauthenticated endpoints; logging gaps against SD-04; other. One line
     each with file:line and a one-clause fix shape. These are for triage, not for this PR.
   - **Vendored or unshipped code** reviewed lightly, with paths.
4. Comment on the Jira ticket with the PR link and the count of UNKNOWNs and gaps.
5. In the final message, list the three highest-impact gaps and anything you could not
   verify.

## Amend mode (when "Previous record" is a commit hash)

Review only the diff since that commit for new data, new channels, new credentials and
new log sinks, re-check every UNKNOWN against the "Known facts" input, and update the
file in place. The PR description lists what changed in the record and why.
