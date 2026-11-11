# Instructions for AI agents

Read this file, then [`docs/PLAN.md`](docs/PLAN.md). Together they override the
README, any older instruction file, and any comment in the code.

## How to pick work

1. Open `docs/PLAN.md` and find the status table at the top.
2. Take the lowest-numbered task whose status is `todo`.
3. Do not start a phase until the previous gate is marked `passed`.
4. Tasks marked NEEDS HUMAN are not yours. Stop and ask the owner to run them.

## Rules

1. **One task, one branch, one PR.** Branch name `task/T<id>-<slug>`, PR into
   `main`. Never push to `main`. Never force-push.
2. **Follow the order.** Lowest-numbered open task first. No phase starts before
   the previous gate is passed.
3. **Stay inside the task.** Change only the files the task lists. If the build
   forces another file, say why in the PR. No extra features, renames,
   dependencies or refactors.
4. **Never type a GUID, API signature or value range from memory.** Use the
   Windows SDK symbol. If there is none, copy from the reference tables in
   `docs/PLAN.md`. If it is not in the tables, stop and ask.
5. **Do not claim hardware results.** CI and virtual machines cannot show that a
   power setting took effect. Write "verified on hardware" only when a person
   has pasted the output of `scripts/verify-hardware.ps1` into the PR.
6. **Documented APIs only.** No `Nt*` calls, no `PowerSetActiveOverlayScheme`,
   no timer-resolution changes, no registry writes to power settings.
7. **Every fix ships with a test** that fails before the change and passes
   after. Tests use the fake backend and must leave `powercfg /list` unchanged.
8. **Wide-character Win32 only.** `W` functions and `std::filesystem::path`. No
   `A` functions, no `MAX_PATH` buffers.
9. **Done means all four:** CI green (Release x64 build, tests, format); the
   task's "Done when" met; no invariant weakened; the task's row updated in
   `docs/PLAN.md` in the same PR.
10. **Stop and report instead of improvising** when a "Done when" cannot be met,
    a source contradicts the plan, the task needs administrator rights or real
    hardware, or a change would touch an invariant.
11. **Leave the user's machine alone.** Never delete a power plan other than
    ours, never kill a process by name alone, never write outside the install
    folder and `%LOCALAPPDATA%\VigilanTune`.
12. **Learning stays inside the fence.** Any learning code may only choose among
    the three profiles. It never writes a raw power value.

## PR description

Always these five lines:

1. Task ID
2. What changed
3. How it was verified, with commands and output
4. What was not verified
5. Invariants touched
