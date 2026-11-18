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

### 4. [done] Document and validate LCD ghosting as an estimate


### 5. [done] Do not change presentation state after a failed state load

### 6. [done] Clarify and test when BIOS selection takes effect


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
