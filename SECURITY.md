# Security Policy

## Supported

Only the latest `main` branch is supported with security fixes. The plugin targets Windows 10/11 x64.

## Reporting a vulnerability

Do **not** open a public issue with exploit details. Report privately using one of:

- A **GitHub private security advisory** (Security tab → "Report a vulnerability") — preferred.
- Direct contact with a maintainer via their GitHub profile.

Please include a description of the issue, steps to reproduce, and the affected commit. We will acknowledge the report and work with you on a fix and disclosure timeline.

## Scope notes

The plugin performs **no telemetry, no network access and no file I/O** in the audio path; state file access happens only in non-real-time host callbacks. Reports involving third-party components should mention the component and version (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
