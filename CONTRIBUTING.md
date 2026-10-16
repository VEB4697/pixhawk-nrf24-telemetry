# Contributing / Working agreement

Solo project today, but written so anyone can reproduce and contribute.

## Branches
- `main` - always builds, tests pass, tagged releases only come from here.
- Short-lived branches, merged by PR (or fast-forward when solo): `feat/<topic>`, `fix/<topic>`, `docs/<topic>`, `test/<topic>`, `hw/<topic>`.
- Example: `feat/uart-bridge`, `fix/ack-fifo-overflow`, `hw/enclosure-v1`.

## Commits (Conventional Commits)
`type(scope): summary` - types: `feat fix docs test chore refactor perf build ci`; scopes: `core air ground radio tools hw docs ci`.
- `feat(core): add packetizer hold timer`
- `fix(radio): re-init when channel register resets`
- `docs(log): day-02 bench results`
- End-of-day: `chore(log): day-NN <one line>` and tag `git tag day-NN`.

## Before every push
1. `pio test -e native` (or run the g++ line in `docs/FIRMWARE_PRINCIPLES.md` if you have no PlatformIO native toolchain).
2. `pio run -e air -e ground` compiles.
3. Docs touched by the change are updated (STATUS, log, DECISIONS if a decision changed).

## Releases
SemVer tags: `v0.1.0` (Day 3 radio throughput), `v0.5.0` (GCS end-to-end), `v1.0.0` (acceptance criteria met), `v1.1.0` (Week 2), `v2.0.0` (PCB). Update `CHANGELOG.md` and `include/version.h`.

## Issue labels (create on GitHub)
`firmware`, `hardware`, `docs`, `test`, `bug`, `risk`, `week-1`, `week-2`, `week-3`, `week-4`, `blocked`.
