# AGENTS.md

**Read [CLAUDE.md](CLAUDE.md) and follow it. It is the whole guide; this file adds nothing.**

That file carries what this project is, where the engine lives, the exact build and
headless-test commands, the module split, the design budgets, the console variables,
and — most importantly — the list of things in the code that look like mistakes and
are not. Most non-obvious code here answers a documented failure mode, and changing it
without that list is how a fix reintroduces a defect that was already paid for once.

## Why this file is one line long

It used to be a copy. Copies of a guide do not stay copies: this one was taken around
2026-09-07, drifted for two days, and by the time it was noticed it still repeated
checkpoint 1's "57 unbroken seconds" — a figure the project had since disproved, since
`UpdateBasedMovement` runs twice per frame and the drift clock was double-counting, so
the real measurement was 28.6 seconds. It was also missing the three failure modes found
after it was copied.

A stale guide is worse than no guide: an agent reading it is confidently wrong instead
of merely uninformed, and the numbers in it look exactly as trustworthy as the ones that
are true. So this file points at the single source instead of restating it. Do not
re-expand it — add to `CLAUDE.md`.
