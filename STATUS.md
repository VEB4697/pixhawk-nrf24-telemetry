# STATUS (single source of truth)

**Last updated:** 2026-09-29 (end of Day 1 build)
**Current phase:** Week 1 / Day 1 complete (software base) -> Day 2 bench bring-up
**Overall health:** GREEN

## Hardware confirmed
Pixhawk 2.4.8 (DF13 6-pin TELEM1) | ESP32-C3 SuperMini x2 | nRF24L01+PA+LNA (owned) | 3D-printed enclosures (self-made)

## Done
- [x] Docs set (PRD, feasibility, architecture, BOM, budget, roadmap, tests, risks, decisions)
- [x] Repo base: name `pixhawk-nrf24-telemetry`, hierarchy, .gitignore/.gitattributes/.editorconfig, LICENSE, CHANGELOG, CONTRIBUTING, CI, issue/PR templates
- [x] PlatformIO workspace: envs `air`, `ground`, `native`
- [x] LinkCore library (frame, ring buffer, packetizer, link stats) - **17/17 host tests pass**
- [x] Day 2 bring-up firmware drafts (PRX/PTX counter exchange) + RF24 wrapper - syntax-checked only
- [x] Docs: REPO_STRUCTURE, FIRMWARE_PRINCIPLES, HARDWARE_SETUP, enclosure brief, mavlink_sniff tool

## In progress / needs you (Day 1 leftovers)
- [ ] `git init` + push to GitHub (commands in `docs/REPO_STRUCTURE.md`)
- [ ] Install PlatformIO, run `pio test -e native` and `pio run -e air -e ground` (first real compile)
- [ ] Confirm you have the nRF24 **socket adapters** (3.3 V regulator) and a DF13 6-pin cable (see BOM)
- [ ] Order any missing small parts (adapters, capacitors, cable, USB-TTL, perfboard)
- [ ] Install Mission Planner or QGC; check Pixhawk firmware type/version
- [ ] Measure boards for enclosure (`hardware/enclosure/README.md`)

## Next actions (Day 2)
1. Wire both units on breadboard with capacitors; antennas on first. Photos to `hardware/wiring/`.
2. Flash `ground` and `air`; run T-01 then T-02 (counter exchange, 5 min, 0 loss).
3. Fix any compile/API issues from the first build; record final pin map confirmation in ADR-007.

## Blockers
- None. Watch: missing adapters/capacitors would block Day 2.

## Open questions
- Do you own the nRF24 socket adapters / DF13 cable? (affects BOM and budget)
- Which Pixhawk firmware + version (ArduPilot/PX4)?
- Which OS/IDE (Windows + VS Code + PlatformIO assumed)?

## Key numbers (fill as measured)
| Metric | Target | Measured |
|---|---|---|
| Throughput (bench, 1 Mbps) | >= 5.8 KB/s | - |
| Packet loss @ 300 m LOS | < 5% | - |
| Latency one-way | < 30 ms | - |
| Range LOS | >= 300 m | - |

## Last decisions
ADR-007 pin map, ADR-008 monorepo with one PlatformIO project, ADR-009 header-only LinkCore, ADR-010 Pixhawk UART on GPIO0/1, ADR-011 drop-newest ring policy (see `docs/DECISIONS.md`).
