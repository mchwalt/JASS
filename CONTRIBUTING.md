# Contributing to JASS

Thanks for your interest! A few honest words about how this project works, so
nobody wastes an evening.

## This is a solo project

JASS is designed, built and ear-tested by one maintainer, at a deliberate pace
and with strong opinions about its architecture (see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) and the reasoning-per-change in
[`CHANGELOG.md`](CHANGELOG.md)).

**Please open an issue to discuss before writing a pull request.** Unsolicited
PRs — especially larger ones — are likely to be declined, not because they are
bad, but because every merged line has to fit the existing contracts (preset
format is append-only, parameter registration order is frozen, UI follows the
module-spec system) and the maintainer has to be able to carry it afterwards.

## Bug reports are very welcome

The most useful report contains:

- the **release version** (title bar / release tag) and your **OS**,
- **standalone or VST3** (VST3 is experimental — say which host),
- steps to reproduce, expected vs. actual,
- if sound-related: the **preset** (`.jass` file) that shows it.

English or German — both fine. / Deutsch ist ebenso willkommen.

## What "tested" means here

There is no automated test rig. Verification is: it builds, the app runs, and
changes are confirmed by eye and by ear against measured references. Keep that
in mind when proposing changes — a patch nobody can hear-verify is hard to
accept.

## Building

See [README → Build & run](README.md#build--run). Windows (MSVC) and Linux are
built by CI on every release; anything else is uncharted territory.
