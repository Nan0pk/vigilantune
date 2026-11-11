# VigilanTune: plan to 1.0

Written 2026-11-11 from a review of `main` at `e0dc506` and branch `vigilantune`
at `c522121`, checked against Microsoft's documentation. Nothing in this plan
was run on Windows. Agents: read [`AGENTS.md`](../AGENTS.md) first.

**The goal.** VigilanTune must be smart, watchful, and measurably better than
leaving Windows alone. If Gate 4 cannot show that, the project stops before
packaging.

## Status

Update your task's row in the same PR that does the work. Status is one of
`todo`, `in review`, `done`; gates are `closed` or `passed`.

| ID | Task | Status | PR |
| --- | --- | --- | --- |
| T0.1 | Agent docs and README banner | in review | |
| T0.2 | Fix the compile error | todo | |
| T0.3 | Park host-changing tests and timing benchmarks | todo | |
| T0.4 | CI: Release build, pinned googletest, plan-list check | todo | |
| T0.5 | Format pass | todo | |
| T0.6 | One name everywhere | todo | |
| T0.7 | Remove stale data; safe uninstall matching | todo | |
| **Gate 0** | Clean base | closed | |
| T1.1 | Power backend interface and fake | todo | |
| T1.2 | Settings from SDK symbols, checked in CI | todo | |
| T1.3 | Own one plan | todo | |
| T1.4 | Replace actuators and control rule (never split) | todo | |
| T1.5 | Restore on exit | todo | |
| T1.6 | Self-test and hardware script | todo | |
| T1.7 | Hardware run (NEEDS HUMAN) | todo | |
| T1.8 | Decide administrator rights | todo | |
| **Gate 1** | Correct, safe actuation | closed | |
| T2.1 | Event-driven policy | todo | |
| T2.2 | Heartbeat that never stops | todo | |
| T2.3 | Watchdog fixes | todo | |
| T2.4 | Restore on every exit path | todo | |
| T2.5 | Paths and Unicode | todo | |
| T2.6 | Config validation | todo | |
| T2.7 | Governor off by default | todo | |
| T2.8 | Fault tests | todo | |
| T2.9 | Laptop checks (NEEDS HUMAN) | todo | |
| **Gate 2** | Reliable lifecycle | closed | |
| T3.1 | Delete the ONNX path | todo | |
| T3.2 | Delete dead telemetry | todo | |
| T3.3 | Foreground app by executable path | todo | |
| T3.4 | Per-app rules | todo | |
| T3.5 | Full-screen detection | todo | |
| T3.6 | Static runtime | todo | |
| **Gate 3** | Old model gone, rules in | closed | |
| T4.1 | Benchmark harness | todo | |
| T4.2 | Demand sampler | todo | |
| T4.3 | Per-app memory | todo | |
| T4.4 | Smart selection | todo | |
| T4.5 | Outcome record | todo | |
| T4.6 | Try and measure (beta) | todo | |
| T4.7 | Background drain alert | todo | |
| T4.8 | Benchmark run (NEEDS HUMAN) | todo | |
| T4.9 | Tune | todo | |
| **Gate 4** | Smart mode beats Windows, or stop | closed | |
| T5.1 | Start at logon | todo | |
| T5.2 | Tray icon | todo | |
| T5.3 | Installer | todo | |
| T5.4 | Release workflow | todo | |
| T5.5 | Documentation | todo | |
| T5.6 | Compatibility matrix (NEEDS HUMAN) | todo | |
| T5.7 | 72-hour soak (NEEDS HUMAN) | todo | |
| **Gate 5** | Release 1.0 | closed | |

## What the review found

### Confirmed in the code

| Finding | Where |
| --- | --- |
| `main` does not compile: `goto cleanup` jumps over initialised variables | `src/agent/inference.cpp` 85-122 |
| Max and min processor state GUIDs are invented, so tuning writes change nothing | `src/agent/actuators.cpp` 16, 18 |
| The branch's EPP and cooling GUIDs are also wrong | branch `actuators.cpp` |
| Writes go to whichever plan is active, including the user's own | `actuators.cpp` 95 |
| Model exports 1 output, controller reads 2; features differ between training and runtime | `models/train.py`, `controller.cpp` 46-60 |
| The committed `telemetry_log.csv` replaces synthetic data in training | `train.py` 56 |
| Model and telemetry paths depend on the working directory | `config.hpp` 11-12, `watchdog/main.cpp` 20 |
| Governor idles every process except the foreground PID and a short name list | `governor.cpp` 77-125 |
| Hangs during start-up are never detected; tick 0 counts as alive | `heartbeat.hpp` 87, `agent/main.cpp` 87 |
| Tests create and activate a real power plan | `tests/test_actuator.cpp`, `test_pipeline.cpp` |
| CI builds Debug and pulls googletest from a moving `main.zip`; uninstall kills any process named `agent` | `build.yml`, `tests/CMakeLists.txt`, `uninstall.ps1` 15 |

### Do not act on these claims from the earlier analysis

| The analysis said | What is true | What to do |
| --- | --- | --- |
| `PowerWriteSettingAttributes` takes 4 arguments, so the branch will not compile | Microsoft documents 3: subgroup, setting, attributes ([source](https://msdn.microsoft.com/en-us/windows/desktop/aa373127)). The branch call is valid. | Do not "fix" the arity. Delete the calls: they un-hide settings system-wide for no benefit. |
| Fix the GUIDs first, rework the control rule in a later step | With real GUIDs, today's rule writes Maximum processor state = 0% whenever stress is 30 or less | Never split these. One PR replaces both (T1.4), or an idle CPU gets pinned at its lowest speed. |
| Self-test by writing a value and reading it back | If a write to an unknown GUID succeeds, the read-back succeeds too | Check the setting exists by enumerating the subgroup, and compare the table with `powercfg /aliases` in CI (T1.2). |
| Merge the branch's brace fix | Only that one fix is safe. The branch half-renames namespaces, adds an undocumented timer-resolution actuator and vendors about 10,000 lines of headers. | Do not merge the branch. |
| Audit the branch for leaked credentials | Searched for key and token patterns: none, only local paths and tool files | No history rewrite needed. Never merge those files. |
| Run it as a Windows service | A service runs in session 0 and cannot see the user's foreground window | Per-user app started at logon (T5.1). |
| A `min > max` interval causes a busy-spin | It only inverts the cadence; a spin needs a value of 0 or less | Validation is still required (T2.6). |

Not checked: the CI status on `main`, and whether `PowerWriteACValueIndex`
reports success for unknown GUIDs. Neither changes the plan.

### Missed by the earlier analysis

| # | Finding | Evidence | Why it matters |
| --- | --- | --- | --- |
| N1 | Energy Saver kills the app. The control loop stops ticking the heartbeat while Battery Saver is on; the watchdog reads that as a hang and terminates the agent after 5 s. Each restart either repeats this until the watchdog gives up, or leaves the agent unmonitored. | `agent/main.cpp` 95-99; `watchdog/main.cpp` 73-75 | Energy Saver replaced Battery Saver in Windows 11 24H2 ([source](https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/energy-saver)). |
| N2 | Nothing is restored on exit. The destructor is empty, the user's original plan is never recorded, and the fail-safe forces Balanced. | `actuators.cpp` 66; `watchdog/main.cpp` 162 | Users lose their plan. Once the GUIDs work, the last written values stay in force. |
| N3 | The two exclusion lists disagree. Built-in defaults omit `agent.exe` and `watchdog.exe`, so without the INI the agent idles itself. The shipped INI omits `svchost.exe`. | `config.hpp` 48; `nanoloop_config.ini` 55 | A self-throttled agent misses heartbeats under load and gets killed. |
| N4 | "Background" is treated as "unimportant". Music, calls, recordings, renders, builds and downloads all get idle priority. Foreground restore forces Normal priority over whatever the app chose. | `governor.cpp` 102, 129-152 | Stutter and slow jobs. |
| N5 | The control design fights the hardware. A 50-500 ms loop rewrites and re-activates the plan, and logs to console and disk on every pass. Modern CPUs already pick frequency themselves; EPP is only a hint. | `agent/main.cpp` 101-131; `logger.hpp` 160-165 | A battery tool that wakes 2-20 times a second costs battery. Microsoft treats processor power policy as static configuration ([source](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/design/dn613985(v=vs.85))). |
| N6 | The agent raises itself to High priority and opts out of efficiency mode | `agent/main.cpp` 175-176 | Wrong for a background utility. |
| N7 | ANSI paths and `MAX_PATH` everywhere; narrow-to-wide conversion by byte copy | `agent/main.cpp` 162-171; `config.hpp` 68 | Breaks when the user name or install path has non-ASCII characters. |
| N8 | Hand-typed GUIDs are the root cause | `actuators.cpp` 14-22 | Use Windows SDK symbols and verify them in CI. |
| N9 | The branch's `GEMINI.md` orders agents to keep the 50-500 ms loop, the zero-allocation hot path and ONNX | branch `GEMINI.md` | Replaced by `AGENTS.md` (T0.1). |
| N10 | The value is unproven, and Windows keeps adding the same features: power modes, Energy Saver, Efficiency mode, and a Low Latency Profile from the June 2026 update ([source](https://www.thewindowsclub.com/enable-processor-performance-boost-mode-in-windows)). EnergyStar already does the governor's job ([source](https://github.com/imbushuo/EnergyStar)). | | Prove it beats Windows before packaging (Phase 4). |

## What version 1.0 is

Version 1.0 is a safe profile switcher that learns. It owns one power plan,
learns on the device which apps need speed and which do not, picks the profile
itself, and always puts the machine back the way it found it. It ships only if
measurement shows it beats leaving Windows alone.

| In 1.0 | Not in 1.0 |
| --- | --- |
| Three profiles: Efficiency, Balanced, Performance | The current ONNX model, or any model trained on made-up data |
| Two levers only: energy preference (EPP) and boost mode, separate values for mains and battery | Writing Maximum or Minimum processor state, or cooling policy |
| Smart mode: learns each app's real demand on this machine and picks the profile, stepping up at once when an app needs speed | A model that writes raw power values; learning only ever chooses among the three profiles |
| Switching on events: mains or battery, battery level, Energy Saver, the user's app rules, full screen, user override | A fast control loop driven by a stress score |
| Watchfulness: an alert when a background app drains the battery, with a one-click limit | Process governor switched on by default |
| A local record of what each choice cost in battery, kept on the machine, as training data for a later model | A shared model trained across many machines; that needs users first |
| Tray icon showing the profile and the reason; installer, clean uninstall, start at logon | Undocumented APIs, timer-resolution changes, Windows service |

Supported systems: Windows 11 22H2 or later on x64 in full; Windows 10 22H2 x64
for profile switching only; ARM64 builds in CI but is not validated.

### Why the old model goes and learning stays

The current ONNX model is trained on data generated from the app's own fallback
rule, so it can only copy that rule. A model is as smart as the feedback it
learns from, and today there is none: no measured battery drain, no measured
slowdown. Phases 1, 2 and 4 build the three things any real model needs: levers
that work, a safe fence, and measured outcomes. Phase 4 then adds learning on
the device and starts the outcome record that a larger shared model can be
trained on after 1.0.

### Invariants

Every invariant has an automated test. A PR that weakens one is rejected.

| # | Invariant |
| --- | --- |
| I1 | The app writes only to the power plan it owns, never to any other plan. |
| I2 | On every exit path the user's original plan becomes active again, and every process the app limited is released: clean exit, crash, kill, logoff, shutdown, uninstall. |
| I3 | Every setting it writes is confirmed to exist first and read back after. A mismatch puts the app in a visible "inactive" state. |
| I4 | Every value is clamped to the range in the reference table before it is written. |
| I5 | An automatic step toward saving waits at least 30 seconds after the last change. A step toward speed is immediate. |
| I6 | When nothing changes, the app writes no log lines and averages under 0.3% of one core over 10 idle minutes. |
| I7 | Everything works when the user name or install path contains non-ASCII characters. |
| I8 | The heartbeat ticks in every state, including Energy Saver and paused. |

## Reference tables

These are the only values agents may use for power settings. Entries marked †
come from memory of the Windows SDK, not from a page opened for the review. The
task that first uses one must confirm it against Microsoft Learn and say so in
the PR.

### Power settings

All settings sit in the processor subgroup
`54533251-82be-4824-96c1-47b60b740d00`, SDK symbol
`GUID_PROCESSOR_SETTINGS_SUBGROUP`.

| Setting (powercfg alias) | SDK symbol † | GUID | Range | Use in 1.0 |
| --- | --- | --- | --- | --- |
| Energy preference (`PERFEPP`) | `GUID_PROCESSOR_PERF_ENERGY_PERFORMANCE_PREFERENCE` | `36687f9e-e3a5-4dbf-b1dc-15eb381c6863` ([source](https://learn.microsoft.com/en-us/answers/questions/3966055/set-processor-performance-energy-preference-policy)) | 0-100; 0 favours performance, 100 favours saving ([source](https://docs.microsoft.com/en-us/windows-hardware/customize/power-settings/options-for-perf-state-engine-perfenergypreference)) | Main lever |
| Boost mode (`PERFBOOSTMODE`) | `GUID_PROCESSOR_PERF_BOOST_MODE` | `be337238-0d82-4146-a960-4f3749d470c7` ([source](https://learn.microsoft.com/windows-hardware/customize/power-settings/options-for-perf-state-engine-perfboostmode)) | Index 0-4: 0 Disabled, 1 Enabled, 2 Aggressive, 3 Efficient Enabled, 4 Efficient Aggressive | Second lever |
| Maximum processor state (`PROCTHROTTLEMAX`) | `GUID_PROCESSOR_THROTTLE_MAXIMUM` | `bc5038f7-23e0-4960-96da-33abaf5935ec` ([source](https://www.thewindowsclub.com/show-or-hide-minimum-and-maximum-processor-state)) | 0-100% | Never written |
| Minimum processor state (`PROCTHROTTLEMIN`) | `GUID_PROCESSOR_THROTTLE_MINIMUM` | `893dee8e-2bef-41e0-89c6-b55d0929964c` (same source) | 0-100% | Never written |
| Balanced plan | `GUID_TYPICAL_POWER_SAVINGS` | `381b4222-f694-41f0-9685-ff5bb260df2e` | | Last-resort fallback only |
| VigilanTune plan | defined in our code | `7fbdc34d-3932-414f-8e34-4c5e095a6e4b` | | The one plan we own |

Boost mode indexes 5 and 6 exist on newer Windows
([source](https://learn.microsoft.com/en-us/dotnet/api/microsoft.windows.eventtracing.power.processorperformanceboostmode)).
Do not use them in 1.0.

### Profiles: starting values

Balanced means "leave the values the user's original plan had", so running the
app in Balanced changes nothing. The other numbers are starting guesses, not
measured; T4.9 tunes them.

| Profile | EPP on mains | EPP on battery | Boost on mains | Boost on battery |
| --- | --- | --- | --- | --- |
| Efficiency | higher of baseline and 60 | higher of baseline and 80 | 3 | 0 |
| Balanced | baseline | baseline | baseline | baseline |
| Performance | 0 | lower of baseline and 25 | 2 | 2 |

Order for I5: Efficiency, then Balanced, then Performance. Moving right is "a
step toward speed".

### API facts

| API | Fact agents must respect |
| --- | --- |
| `PowerWriteSettingAttributes` | Three parameters. Not called anywhere in 1.0. |
| `PowerDuplicateScheme` | The third parameter is a pointer to a GUID pointer. Point it at our fixed GUID to create the plan under that GUID; it returns `ERROR_ALREADY_EXISTS` if the plan is already there ([source](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-powerduplicatescheme)). |
| `PowerSetActiveScheme` | Changes to the active plan take effect only after this call ([source](https://msdn.microsoft.com/en-us/library/aa373120(v=vs.85).aspx)). Call it once per profile change. |
| Access rights | Non-privileged users can change power settings by default ([source](https://learn.microsoft.com/en-us/windows/win32/Power/power-policy-settings)). Aim for no administrator rights (T1.8). |
| Energy Saver | Register for both `GUID_POWER_SAVING_STATUS` and `GUID_ENERGY_SAVER_STATUS` †. If the second fails to register, treat it as an older Windows and carry on. |
| Power source and battery level | Register for `GUID_ACDC_POWER_SOURCE` and `GUID_BATTERY_PERCENTAGE_REMAINING` † with `RegisterPowerSettingNotification`. |
| `PowerEnumerate` with `ACCESS_INDIVIDUAL_SETTING` † | Lists the settings in a subgroup. Use it for the "setting exists" check in I3. |
| `powercfg /aliases` † | Prints every alias with its GUID. CI compares our table against it. |
| `QueryFullProcessImageNameW` † | Returns a process's executable path. Use it to identify the foreground app. |
| `SHQueryUserNotificationState` † | Reports full-screen and presentation states. |
| `QueryUnbiasedInterruptTime` † | A clock that does not advance during sleep. Use it for heartbeat timestamps. |
| `GetProcessTimes` † | CPU time used by a process. Use it for the drain alert. |

## Phase map

| Phase | Goal | Gate to pass before the next phase | Needs a person |
| --- | --- | --- | --- |
| 0 | Clean base | CI green; tests leave power plans untouched; one name everywhere | No |
| 1 | Correct, safe actuation | Hardware report shows values change per profile and the original plan returns on exit | Yes |
| 2 | Reliable lifecycle | Fault tests green; kill, Energy Saver and sleep checks pass on a laptop | Yes |
| 3 | Remove the old model, add app rules | No ONNX left; rule tests green | No |
| 4 | Smart mode, then proof | Benchmark shows Smart mode beats Windows left on Balanced; if not, stop | Yes |
| 5 | Package and release | The 1.0 checklist is complete | Yes |

## Phase 0: clean base

Makes `main` build, makes CI trustworthy and gives agents one set of
instructions. Changes no behaviour.

| ID | Do | Files | Done when |
| --- | --- | --- | --- |
| T0.1 | Add `docs/PLAN.md`, `AGENTS.md`, and `GEMINI.md` and `CLAUDE.md` pointing to it. Put a pre-alpha banner at the top of the README. | new files, `README.md` | Files exist on `main`; banner visible |
| T0.2 | Fix the compile error. Enclose everything from the two `std::vector` declarations to just before `cleanup:` in one `{ }` block. Change nothing else. | `src/agent/inference.cpp` | `cmake -B build -A x64 -DNANOLOOP_DISABLE_AI=ON` then `cmake --build build --config Release` succeed in CI |
| T0.3 | Move `test_actuator.cpp` and `test_pipeline.cpp` into a second test program that CI does not run. Give the timing tests in `test_benchmark.cpp` the ctest label `benchmark` and exclude that label by default. | `tests/CMakeLists.txt` | `ctest -C Release` runs neither group |
| T0.4 | CI: build with `--config Release`; run `ctest -C Release --output-on-failure`; pin googletest to a release tag with `URL_HASH SHA256=`; save `powercfg /list` before and after the tests and fail if they differ; upload the binaries as an artifact. | `.github/workflows/build.yml`, `tests/CMakeLists.txt` | Build and test jobs green; the plan-list check passes |
| T0.5 | Run `clang-format-17` over `src` and `tests`. Formatting only. | `src/**`, `tests/**` | Format job green; the diff has no logic changes |
| T0.6 | One name. Namespace `vigilantune`; programs `VigilanTune.Agent.exe` and `VigilanTune.Watchdog.exe`; config `vigilantune.ini`; plan name "VigilanTune"; README badge and links point to `Nan0pk/vigilantune`. Keep the text "Nanoloop AI Optimized" in one place only: a list of legacy plan names to clean up. | all source, CMake, scripts, README | `git grep -i -E "nanoloop\|powerwatcher\|wspa"` shows only that list, the changelog and this plan; CI green |
| T0.7 | Delete `models/telemetry_log.csv` and ignore it in `.gitignore`. In `uninstall.ps1`, stop processes by full path inside the install folder, never by the name `agent`. | `models/`, `.gitignore`, `scripts/uninstall.ps1` | File gone; script matches on path |

The `vigilantune` branch stays unmerged.

**Gate 0:** on `main`, the Release x64 build, tests and format jobs are green,
the plan-list check passes, and the name search is clean.

## Phase 1: correct and safe actuation

The app changes real settings safely: it owns one plan, writes only EPP and
boost mode, proves each write, and restores the user's plan on exit.

| ID | Do | Files | Done when |
| --- | --- | --- | --- |
| T1.1 | Add `IPowerBackend` with two implementations, `Win32PowerBackend` and `FakePowerBackend`. Methods: get active plan; duplicate a plan to a given GUID; set active; delete plan; set plan name; read and write mains and battery values; list the settings in a subgroup. `ActuatorManager` receives the backend in its constructor. Move the two parked test files back and run them on the fake. | new `src/agent/power_backend.*`, `actuators.*`, `tests/` | `git grep` finds no `Power*` call outside `Win32PowerBackend`; tests green; plan-list check passes |
| T1.2 | Add `power_settings.hpp` holding only the subgroup, EPP and boost mode, by SDK symbol. Delete every hand-typed `DEFINE_GUID`. Add a unit test that compares each symbol with the GUID text in the reference table. Add a CI step that compares them with `powercfg /aliases`: fail on a different GUID, warn if the runner lacks an alias. | `power_settings.hpp`, `actuators.cpp`, `build.yml`, `tests/` | Both checks pass in CI |
| T1.3 | Own one plan. See "T1.3 in detail". | `actuators.*`, new `state.*` | Fake-backend tests prove: no write targets another GUID (I1); `state.json` holds the original before our plan is activated; an outside plan change causes zero writes |
| T1.4 | Replace the actuators and the control rule together. See "T1.4 in detail". **Never split this task.** | `actuators.*`, `controller.*`, `types.hpp`, `agent/main.cpp`, `tests/` | Tests cover clamping (I4), exists-then-read-back (I3) and failure handling; `git grep -i throttle` finds no power write |
| T1.5 | Restore on exit. Add `ActuatorManager::shutdown()`: activate the original plan from `state.json`; if it no longer exists, activate Balanced; mark the state clean. Call it on normal exit. At start-up, if the state is not clean, restore first, then start. | `actuators.*`, `state.*`, `agent/main.cpp` | Fake-backend tests for clean exit, missing original and unclean start |
| T1.6 | Add `--selftest` to the agent: apply each profile, print the values read back, restore, exit 0 or 1. Add `scripts/verify-hardware.ps1`: print Windows build, CPU, `powercfg /a`, active plan and `powercfg /list`; run the self-test; print `powercfg /qh` for EPP and boost mode per profile; print the active plan and plan list again; end with PASS or FAIL per check in one text block. | `agent/main.cpp`, `scripts/` | Script completes on the CI runner and prints the report |
| T1.7 NEEDS HUMAN | Run `verify-hardware.ps1` on a real laptop from a normal, non-administrator terminal, once on mains and once on battery. Paste both reports into the PR. Also check by eye: with the app running, can Settings > System > Power & battery > Power mode still be changed? | | Both reports attached; every check PASS; the Power mode answer recorded |
| T1.8 | If T1.7 passed without administrator rights, set both manifests to `asInvoker`. Otherwise keep `requireAdministrator` and record the failing step here. | both `.manifest` files | Decision recorded |

### T1.3 in detail

1. Keep state in `%LOCALAPPDATA%\VigilanTune\state.json`. Write it to a
   temporary file, then rename over the old one.
2. First run: read the active plan. That is the "original". Save its GUID to the
   state file before doing anything else.
3. If the active plan is ours or a legacy "Nanoloop AI Optimized" plan and no
   state file exists, use Balanced as the original.
4. Duplicate the original into our fixed GUID from the reference table and name
   it "VigilanTune". If our plan already exists, reuse it.
5. Read the original's EPP and boost values for mains and battery and store them
   as the baseline.
6. Activate our plan.
7. Delete `refresh_active_scheme()` from the write path, and delete the branch
   that falls back to writing the active plan.
8. If any other plan becomes active while the app runs, stop writing, show
   "Paused: power plan changed outside VigilanTune", and do not switch back.

### T1.4 in detail

1. Remove `PerformanceBoost`, `ProcessorFloor`, the deadband code,
   `commit_changes(stress)` and the stress-to-value rule in
   `compute_adjustments`.
2. Add `struct Profile { epp_ac; epp_dc; boost_ac; boost_dc; }` and
   `apply_profile(Profile)`.
3. `apply_profile` does exactly this, in order: confirm both settings exist;
   clamp to the reference ranges; write the four values to our plan; call
   `PowerSetActiveScheme` once; read the four values back and compare.
4. On any failure, write nothing more and enter the inactive state with a logged
   reason.
5. Until Phase 2 lands, the agent applies Balanced once at start and does
   nothing else.

**Gate 1:** reports from at least one laptop show that EPP and boost values
differ per profile on mains and on battery, the original plan is active after
exit, and the plan list gained only "VigilanTune".

## Phase 2: reliable lifecycle

Replaces the fast loop with event-driven switching and makes every way of
stopping the app end with the user's plan restored.

| ID | Do | Files | Done when |
| --- | --- | --- | --- |
| T2.1 | Event-driven policy replaces the 50-500 ms loop. See "T2.1 in detail". | `agent/main.cpp`, `controller.*`, `sensors.*`, `tests/` | Table-driven tests map contexts to profiles; the step-down delay holds (I5); an idle minute produces zero log lines (I6) |
| T2.2 | A heartbeat that never stops. Tick once a second from the policy thread in every state. Use `QueryUnbiasedInterruptTime` for timestamps on both sides. Energy Saver selects Efficiency instead of pausing the loop. | `heartbeat.hpp`, `agent/main.cpp` | Test: saver on for 30 s, heartbeat keeps advancing, profile is Efficiency (I8) |
| T2.3 | Watchdog fixes. See "T2.3 in detail". | `watchdog/main.cpp`, `heartbeat.hpp`, `agent/main.cpp` | Covered by T2.8 |
| T2.4 | Every exit path restores. Handle `CTRL_CLOSE_EVENT`, `CTRL_LOGOFF_EVENT`, `CTRL_SHUTDOWN_EVENT`, `WM_QUERYENDSESSION` and `WM_ENDSESSION`; each calls `shutdown()` from T1.5. | `agent/main.cpp` | Covered by T2.8 and T2.9 |
| T2.5 | Paths and Unicode. Replace every `A` function. Read the default config from the program folder. Write logs, state and the user's config to `%LOCALAPPDATA%\VigilanTune`. Let the environment variable `VIGILANTUNE_BACKEND=fake:<file>` select a fake backend that saves to that file, for tests only. | all source | CI copies the build into a folder with non-ASCII characters in its name and `--selftest` passes on the fake (I7) |
| T2.6 | Config validation. One table lists each key with type, minimum, maximum and default. Out of range: clamp and warn. Unknown key: warn. Accept `;` and `#` comments, at line start and inline. Make the log level work at run time. Remove `#undef ERROR`. | `config.hpp`, `config_loader.hpp`, `logger.hpp`, the INI | One unit test per rule |
| T2.7 | Governor off. Start `governor_thread` only when `governor.enabled = true`; default `false`; mark it experimental in the INI. Remove `SetPriorityClass(HIGH_PRIORITY_CLASS)` and `DisableEfficiencyMode()` from the agent. | `agent/main.cpp`, `config.hpp`, the INI | A default run starts no governor thread; a test asserts it |
| T2.8 | Fault tests on the fake backend, with a test-only switch `VIGILANTUNE_TEST_FREEZE=init` or `loop`. See "T2.8 cases". | `tests/integration/` | All six cases green in CI |
| T2.9 NEEDS HUMAN | Run the five laptop checks under "T2.9 checks" and paste the results into the PR. | | All five PASS |

### T2.1 in detail

1. Delete the 50-500 ms loop and remove the stress score from every decision.
   The policy thread wakes only for an event or the one-second heartbeat timer.
2. The context has six fields: on mains; battery percent; Energy Saver on;
   foreground-app rule; full screen; user override. The last three stay empty
   until Phases 3 and 5.
3. `select_profile(context)` is a pure function. First match wins: user
   override; Energy Saver on gives Efficiency; app rule; full screen gives
   Performance on mains and Balanced on battery; on battery below 30% gives
   Efficiency; otherwise Balanced.
4. Apply a profile only when it differs from the current one. A step toward
   saving also needs 30 seconds since the last change; a step toward speed is
   immediate.
5. Events come from `RegisterPowerSettingNotification` for power source, battery
   percentage and both saver settings, plus the existing foreground hook.
6. Log one line per profile change, with the reason. Log nothing per tick.
7. Stop collecting thermal, GPU, disk, frequency and timer counters.

### T2.3 in detail

1. The watchdog creates the heartbeat mapping before it starts the agent; the
   agent only opens it.
2. No first tick within 30 s of start is a hang. After the first tick, 10 s
   without a tick is a hang.
3. Start the agent with `CreateProcessW`, working directory set to the program
   folder, and pass the watchdog's process ID on the command line.
4. The agent opens a handle to that process ID and waits on it. If the watchdog
   dies, the agent calls `shutdown()` and exits. Delete `IsWatchdogRunning`.
5. One instance per machine: mutexes `Global\VigilanTune.Agent` and
   `Global\VigilanTune.Watchdog`. A second instance exits with code 2.
6. On a crash or hang: terminate the agent if needed, restore the original plan
   from `state.json` (Balanced only if it is missing), wait 2, 4, then 8 s,
   restart. After the last attempt, leave the original plan active and exit.
7. Agent exit code 0 means the user chose to quit: do not restart.

### T2.8 cases

1. Kill the agent: original plan restored within 10 s.
2. Freeze the loop: detected within 20 s, original restored.
3. Freeze during start-up: detected within 40 s, original restored.
4. Energy Saver simulated on for 60 s: agent still running.
5. Start a second agent: it exits with code 2.
6. Kill the watchdog: the agent restores the original and exits within 10 s.

### T2.9 checks

1. `taskkill /f` the agent: `powercfg /getactivescheme` shows the original plan
   within 10 s.
2. Energy Saver on for 10 minutes: both programs still running.
3. Sleep and resume three times: both running, correct profile.
4. Sign out and back in: the original plan was restored and the state file is
   clean.
5. Close the console window: the original plan is restored.

**Gate 2:** T2.8 is green in CI and T2.9 passes on a laptop.

## Phase 3: remove the old model, add app rules

Deletes the model path, which cannot learn anything useful as built, and adds
per-app rules and full-screen detection. Learning returns in Phase 4, built on
measured outcomes instead of made-up data.

| ID | Do | Files | Done when |
| --- | --- | --- | --- |
| T3.1 | Delete the ONNX path: `inference.*`, `models/`, the `onnxruntime` dependency in `vcpkg.json` and CMake, the `NANOLOOP_DISABLE_AI` flag, model hash and model path settings, every `PowerSetRequest` call, telemetry CSV logging and rotation, `test_inference.cpp`. Add a changelog entry. | as listed | `git grep -i -E "onnx\|inference\|model_path"` finds nothing outside the changelog and this plan; CI green |
| T3.2 | Delete dead telemetry: stress score, dirty-flag check, the thermal, GPU, disk, frequency and timer tags, their tests, and the timing benchmarks. Delete `TagDatabase` if nothing uses it afterwards. | `sensors.*`, `controller.*`, `types.hpp`, `tests/` | No unused tag remains; CI green |
| T3.3 | Identify the foreground app by executable path, not window title. From the foreground window take the process ID, then call `QueryFullProcessImageNameW`. If the path cannot be read, or the process is `ApplicationFrameHost.exe`, report "no rule". Put this behind `IForegroundProvider` with a fake. | `sensors.*`, new `foreground_provider.*` | Unit tests on the fake; `git grep GetWindowText` is empty |
| T3.4 | Per-app rules. A `[rules]` section in the user's config: `name.exe = efficiency`, `balanced` or `performance`; a full path is also accepted. Matching ignores case; a full path beats a file name. The result fills the app-rule field from T2.1. | `config*.hpp`, `controller.*` | Tests: match by name, match by path, precedence, an invalid profile name is rejected with a warning |
| T3.5 | Full-screen detection with `SHQueryUserNotificationState`. Query on each foreground change, and every 5 s while the answer is full screen, presentation or busy. The result fills the full-screen field from T2.1. | `sensors.*` | Unit tests through a fake |
| T3.6 | Link the C runtime statically (`/MT`) so users need no Visual C++ redistributable. Add a CI step: `dumpbin /dependents` on both programs lists only Windows system DLLs. | CMake files, `build.yml` | CI step passes |

**Gate 3:** the ONNX search is clean, all tests are green, and the dependency
check passes.

## Phase 4: smart mode, then proof

Builds the part that makes the app worth having, learning on the device, and
then tests whether it beats leaving Windows alone. Packaging starts only if it
does.

The learning is deliberately simple and can only choose among the three
profiles, so its worst mistake is the wrong profile for a few seconds. A larger
shared model comes after 1.0, trained on the outcome record this phase starts
keeping.

| ID | Do | Files | Done when |
| --- | --- | --- | --- |
| T4.1 | Benchmark harness `bench/run.ps1` with four workloads: idle desktop for 15 minutes; local video for 15 minutes; a burst test that times `Compress-Archive` on a generated 500 MB folder, five times; a 45-minute "day" that cycles video, idle, the burst, and opening and closing Notepad, Explorer and the default browser on local pages. Per run, record battery energy at start and end from the `root\wmi` class `BatteryStatus`, field `RemainingCapacity` in mWh †, plus elapsed and burst times. Average watts = energy used / hours. Write one CSV row per run and a summary table. | `bench/` | A two-minute dry run completes on the CI runner |
| T4.2 | Demand sampler. On each heartbeat tick, record overall CPU busy percent and whether any single core was above 90% busy. The second is "heavy" when overall CPU is 50% or more, or any core is above 90%. Use a documented counter API † behind `IDemandProvider`, with a fake. | new `demand.*` | Unit tests on the fake; I6 still holds with the sampler running |
| T4.3 | Per-app memory. For the foreground app, by executable path, keep `seconds_observed` and `heavy_share`, a running average of "heavy" between 0 and 1. Store it in `%LOCALAPPDATA%\VigilanTune\apps.json`; save at most once every 5 minutes and on exit, by temporary file and rename. `classify(stats)` is a pure function: under 10 minutes observed gives "unknown"; `heavy_share` below 0.05 gives "light"; above 0.40 gives "heavy"; otherwise "mixed". | new `app_memory.*` | Tests for each class, the 10-minute rule, and a corrupt file being ignored safely |
| T4.4 | Smart selection. See "T4.4 in detail". | `controller.*`, tray text | Table-driven tests for every rule; I5 holds |
| T4.5 | Outcome record, local only. On battery, for every 5 minutes one app stays in the foreground, append a row to `outcomes.csv`: app, profile, average discharge in mW, `heavy_share`. Cap the file at 10 MB. Nothing is uploaded. | new `outcomes.*`, `PRIVACY.md` | Tests for the row format and the size cap |
| T4.6 | Try and measure, a beta that is off by default (`smart.try_and_measure`). On battery, for apps classed "mixed", run 1 in 10 five-minute periods on Efficiency. After 20 periods each way, reclassify the app as "light" if Efficiency used at least 5% less power and `heavy_share` rose by less than 0.05. | `controller.*`, `app_memory.*` | Pure-function tests on recorded periods; off by default |
| T4.7 | Background drain alert. On battery, every 30 s, measure CPU use per process with the existing process snapshot and `GetProcessTimes` †. If a process that is not the foreground app averages over 20% of total CPU for 5 minutes, show one tray notification naming it, with three choices: Ignore once, Always ignore, Limit it. "Limit it" turns on efficiency mode for that process only. Record its process ID in `state.json` and undo the limit when it comes to the foreground, when the app exits, or after a crash. | new `watch.*`, tray | Tests on a fake process list; a limited process is always released (part of I2) |
| T4.8 NEEDS HUMAN | On battery, brightness fixed at 50%, Wi-Fi on, no other apps open. First run the "day" workload once with the app on, unmeasured, so Smart mode can learn. Then run all four workloads twice in each of three set-ups: app off with Windows power mode Balanced; app off with Windows "Best power efficiency"; app on in Smart mode. Run a third time wherever two results differ by more than 3%. Expect about nine hours across several charges. | | `bench/results.csv` and the summary pasted into the PR |
| T4.9 | Tune. Change only the profile numbers and the T4.3 thresholds, inside the reference ranges, using T4.8. Re-run the affected rows. Two tuning rounds at most. | constants, `docs/BENCHMARKS.md` | Final numbers and results recorded |

### T4.4 in detail

1. Add a seventh context field: the learned class of the foreground app.
2. New order for `select_profile`, first match wins: user override; Energy Saver
   gives Efficiency; the user's app rule; full screen; on battery below 30%
   gives Efficiency; learned class; otherwise Balanced.
3. The learned class maps to a profile like this. "Light" gives Efficiency on
   battery and Balanced on mains. "Heavy" gives Balanced on battery and
   Performance on mains. "Mixed" and "unknown" give Balanced.
4. Burst guard: if "heavy" is true for 5 seconds in a row while the profile is
   Efficiency, step up to Balanced at once. I5 allows this.
5. Smart mode has one switch, `smart.enabled`, on by default. Off means rules
   only.
6. The tray reason names the cause in plain words, for example "Efficiency:
   Notepad rarely needs full speed".

**Gate 4, go or stop:** Smart mode must do both of these against Windows left
on Balanced. It uses at least 5% less average power on the idle, video and "day"
workloads. It is no more than 5% slower on the burst test. If it fails after two
tuning rounds, stop: do not start Phase 5, write up the results in
`docs/BENCHMARKS.md`, and ask the owner what to change.

## Phase 5: package and release

Turns the working core into something a stranger can install, understand and
remove without a trace.

| ID | Do | Files | Done when |
| --- | --- | --- | --- |
| T5.1 | Start at logon without a Windows service. If T1.8 chose `asInvoker`, the installer adds a per-user Run entry that starts the watchdog. Otherwise it adds a Scheduled Task with highest privileges. | `installer/` | Signing in starts both programs with no prompt |
| T5.2 | Tray icon inside the agent, plain Win32 `Shell_NotifyIconW`, no UI framework. First menu line and tooltip: profile and reason, for example "Efficiency: on battery, 24%". Menu: Auto, Efficiency, Balanced, Performance (these set the user override from T2.1); Smart mode on or off; Pause; Open log folder; Open settings file; Quit, which restores the original plan and exits with code 0. Declare per-monitor DPI awareness v2. Build as a Windows-subsystem program so no console appears. | `agent/`, manifest | Screenshots at 100% and 150% scaling in the PR; menu actions covered by tests on the policy function |
| T5.3 | Installer: an MSI built with WiX 4 or later. Per-user if `asInvoker`, per-machine otherwise. An upgrade keeps the user's config and learned data. Uninstall stops both programs by path, restores the original plan, deletes our plan by GUID and any legacy "Nanoloop AI Optimized" plans, removes the logon entry, and asks before deleting `%LOCALAPPDATA%\VigilanTune`. Delete `uninstall.ps1`. | `installer/`, `build.yml` | CI installs, runs and uninstalls on the runner; `powercfg /list` and the active plan are identical before and after |
| T5.4 | Release workflow. A `v*` tag builds Release x64, runs every test, builds the MSI, and attaches the MSI and its SHA-256 to a GitHub Release. The version lives in one place: `project(VERSION ...)`. Add ARM64 as a compile-only job. | `.github/workflows/`, CMake | Tagging `v0.9.0-rc1` produces a release with both files |
| T5.5 | Documentation. Rewrite the README to match reality: what the app does, what it does not do, the measured results from Gate 4, and how to restore by hand with `powercfg /setactive SCHEME_BALANCED`. Add `PRIVACY.md` (nothing leaves the machine) and `TROUBLESHOOTING.md`. Remove the SCADA and nanosecond claims. | docs | Every README claim points to a test or a benchmark |
| T5.6 NEEDS HUMAN | Compatibility matrix in `docs/COMPAT.md`. On each machine below: install the MSI, run `verify-hardware.ps1`, run the T2.9 checks, uninstall, confirm the plan list is as before. | `docs/COMPAT.md` | Every row PASS, or the failure is fixed, or it is listed in the README as a known limitation |
| T5.7 NEEDS HUMAN | Soak for 72 hours on a laptop in daily use; `scripts/soak-report.ps1` samples every 5 minutes. Pass: no crash, no watchdog recovery, memory growth under 5 MB, average CPU under 0.3% of one core, stable handle count. | `scripts/` | Report attached; every line passes |

### Minimum compatibility matrix

1. Intel laptop with performance and efficiency cores, Windows 11 24H2 or later.
2. AMD Ryzen laptop, Windows 11.
3. Desktop with no battery, Windows 11.
4. Windows 10 22H2, real or virtual, functional checks only.
5. A laptop where `powercfg /a` lists "S0 Low Power Idle" (Modern Standby).
6. A laptop running its maker's power tool, such as HP OMEN, Lenovo Vantage,
   ASUS Armoury Crate or Dell. Expected: when that tool changes the plan,
   VigilanTune pauses.
7. A standard, non-administrator account whose name has non-ASCII characters.
8. Two users signed in at once. Expected: the second session's agent exits with
   code 2.

### Definition of done for 1.0

- [ ] Gates 0 to 5 passed and recorded in the status table
- [ ] Invariants I1 to I8 each have a named test, all green
- [ ] Install, use and uninstall leave the plan list and active plan unchanged
      on every matrix machine
- [ ] 72-hour soak passed
- [ ] README claims match the Gate 4 results
- [ ] The release was built by the tag workflow and carries a checksum

## Decisions for the owner

Agents proceed on the defaults below unless the owner changes them. D5 and D7
need an answer before 1.0.

| # | Decision | Default assumed | If the owner chooses otherwise |
| --- | --- | --- | --- |
| D1 | Product name | VigilanTune everywhere | Only the text in T0.6 changes |
| D2 | What "smart" means in 1.0 | Remove the current ONNX model. Add on-device learning that picks among three safe profiles (Phase 4). | Keeping the ONNX model ships a part that only copies a simple rule |
| D3 | Supported Windows | 11 22H2 or later on x64 in full; 10 22H2 for switching only | More matrix rows |
| D4 | Process governor | Off by default. The drain alert limits one app at a time, and only on the user's click. | Backlog item B1 must land first |
| D5 | Code-signing certificate | None yet | Without one, Windows SmartScreen warns users at install. It costs money; decide before 1.0. |
| D6 | The Gate 4 bar | At least 5% less power than Windows left on Balanced, at most 5% slower. Failing it stops the project before packaging. | Edit the gate text |
| D7 | Who runs the NEEDS HUMAN steps, and on which machines | The owner, on their own laptop first | Matrix rows with no machine are listed as untested in the README |
| D8 | Licence | Stays MIT; no code copied from GPL projects such as EnergyStarX | |

Owner time, roughly: 20 minutes for T1.7, 30 minutes for T2.9, nine hours on
battery for T4.8, an hour per machine for T5.6, and 72 unattended hours for
T5.7.

## Backlog after 1.0

None of these may start before the 1.0 release.

| # | Item | Note |
| --- | --- | --- |
| B1 | Safe governor | Current session only; exempt the foreground app's whole process family, anything playing audio and anything with a rule; efficiency mode only; record and undo every change; idle on mains. EnergyStar's listed limitations show the traps ([source](https://github.com/imbushuo/EnergyStar)). |
| B2 | ARM64 validated on a Snapdragon laptop | Compile-only until then |
| B3 | Follow the user's Windows power mode | Read it through documented notifications only |
| B4 | Load-aware switching beyond the burst guard | Only if Phase 4 numbers justify it |
| B5 | A shared decision model | Trained on outcome records from many machines, shared only by choice. It needs 1.0 in users' hands first. |
| B6 | Auto-update and a winget entry | After signing is settled |

## Sources

- [PowerWriteSettingAttributes](https://msdn.microsoft.com/en-us/windows/desktop/aa373127), Microsoft
- [PowerDuplicateScheme](https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-powerduplicatescheme), Microsoft
- [PowerWritePossibleValue](https://msdn.microsoft.com/en-us/library/aa373120(v=vs.85).aspx), Microsoft, for the rule that changes need `PowerSetActiveScheme`
- [PERFBOOSTMODE](https://learn.microsoft.com/windows-hardware/customize/power-settings/options-for-perf-state-engine-perfboostmode), Microsoft
- [PerfEnergyPreference](https://docs.microsoft.com/en-us/windows-hardware/customize/power-settings/options-for-perf-state-engine-perfenergypreference), Microsoft
- [MaxPerformance](https://learn.microsoft.com/en-us/windows-hardware/customize/power-settings/options-for-perf-state-engine-maxperformance) and [MinPerformance](https://learn.microsoft.com/en-us/windows-hardware/customize/power-settings/options-for-perf-state-engine-minperformance), Microsoft
- [powercfg output showing the PERFEPP GUID](https://learn.microsoft.com/en-us/answers/questions/3966055/set-processor-performance-energy-preference-policy), Microsoft Q&A
- [Minimum and maximum processor state GUIDs](https://www.thewindowsclub.com/show-or-hide-minimum-and-maximum-processor-state), The Windows Club
- [ProcessorPerformanceBoostMode enum](https://learn.microsoft.com/en-us/dotnet/api/microsoft.windows.eventtracing.power.processorperformanceboostmode), Microsoft
- [Power Policy Settings](https://learn.microsoft.com/en-us/windows/win32/Power/power-policy-settings), Microsoft
- [Processor power policy on Windows](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/design/dn613985(v=vs.85)), Microsoft
- [Energy saver](https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/energy-saver), Microsoft
- [Processor performance boost mode and the Low Latency Profile](https://www.thewindowsclub.com/enable-processor-performance-boost-mode-in-windows), The Windows Club
- [EnergyStar](https://github.com/imbushuo/EnergyStar) and [EnergyStarX](https://github.com/JasonWei512/EnergyStarX)
