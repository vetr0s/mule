Closes #

## What and why

<!-- The change in a few sentences, and the reason for it. Name the
approach you rejected, if there was one. -->

## Verification

<!-- The commands you ran and what they showed. Name the test that failed
before the change and passes after it. Say what you could not check. -->

- [ ] `./build format --check`, `./build ci` and `./build docs` pass
- [ ] `dist/mule.h` is regenerated in each commit that changes `src/`
- [ ] `CHANGELOG.md` has a line, if a user of the library would notice
- [ ] AGENTS.md is updated, if a convention or pitfall changed

## For the reviewer

<!-- Where to start reading, what is riskiest, and what is left for a
follow-up issue. -->
