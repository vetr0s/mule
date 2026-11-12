# Security policy

MULE is a C library, so a memory-safety bug in it can become a security
bug in any program that uses it. Report one privately.

## Supported versions

Only the latest release gets fixes. A fix ships as a new patch release,
with an advisory that names the affected versions.

## Reporting a vulnerability

Use [private vulnerability reporting](https://github.com/vetr0s/mule/security/advisories/new).
Do not open a public issue for it.

Include the MULE version, the platform and compiler, and the smallest
program that shows the problem. A sanitizer report helps most.

MULE is maintained by one person in their free time. Reports are read
and answered as time allows.
