# To-do

## PR #1 follow-up

### 1. [done] Fix tap-latch save-state determinism

### 2. [done] Add integration coverage for every core option

### 3. Make BIOS intro skipping bounded and failure-aware

`runIntro()` currently runs up to 20 million instructions, returns no status, and silently continues
from an arbitrary BIOS location if cartridge entry `$6020` is never reached.

- Return an explicit success/failure result.
- Use a clearly justified instruction- or clock-based limit.
- Avoid a long unexplained stall for invalid, damaged, or unusual content.
- On failure, preserve a safe boot path and report a useful frontend message.
- Add success, timeout, load, and reset tests.

Done when intro skipping either reaches the authentic cartridge-entry state or fails predictably
without silently changing normal boot behavior.

### 4. Document and validate LCD ghosting as an estimate

The five ghosting levels use hand-selected darkening and fading coefficients without a documented
derivation or hardware measurement. The current wording can imply more physical accuracy than has
been established.

- Record the coefficients, recurrence, effective per-frame retention, and source limitations in
  `src/gamate/SOURCES.md`.
- Describe the option as an estimated display-response filter unless measurements justify stronger
  wording.
- Give levels meaningful labels or document what increasing values do.
- Validate the filter against the captured Brick Blaster erase/redraw sequence.
- Add deterministic tests for response direction, endpoints, disabled identity, and level ordering.
- Keep raw instantaneous output available.

Done when the option's behavior and evidentiary status are clear, reproducible, and tested.

### 5. [done] Do not change presentation state after a failed state load

### 6. Clarify and test when BIOS selection takes effect

Changing the BIOS option does not replace the BIOS already held by the loaded machine, so a core
reset continues using the previous BIOS. The current option description says only that the choice
applies when the game restarts.

- Decide whether changing BIOS requires reloading content or should apply on core reset.
- Make the option description explicit.
- Implement safe BIOS reload-on-reset only if that is the intended behavior.
- Test selection changes, reset, content reload, and preferred-file fallback.

Done when the UI text, implementation, and tests agree about the BIOS-change lifecycle.

### 7. Complete source and contributor documentation for PR #1

PR #1 introduced new empirical behavior and code authored by SundownerSport, but did not add the
corresponding source/method notes or contributor attribution.

- Document how the quick-tap polling figures were measured and which titles/states were tested.
- Document the Brick Blaster observation motivating LCD persistence.
- Record that ghosting coefficients are estimates unless later replaced by measured values.
- Credit SundownerSport for the contributed options and implementation without attributing that
  code to Ry.

Done when `src/gamate/SOURCES.md` identifies the evidence, method, limitations, and authorship of
the merged work.
