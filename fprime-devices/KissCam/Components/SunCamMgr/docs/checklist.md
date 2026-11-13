# F´ (F Prime) Component Development Checklist

**Component Name:** SunCamMgr  
**Component Owner:** _[Owner Name]_  
**Component Contributors:** _[Contributor Names]_  
**Review Date:** 2026-11-13  
**Checklist Version:** 1.2.0

_Pre-filled by fprime-auto-checklist 0.1.0 for `lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr` at `69c71bf16019`. 🟢 YES 48, 🟠 NO 14, ⚪ N/A 1, 🔵 REVIEW 5, ⚫ UNCHECKED 5._
_Section fingerprints (inputs each section judges): 1=3a517334b335 2=22356b5a2956 3=c339e82a56c3 4=db8e3700c740 5=e2eca10c892b_
_Stage: **design** (design (SDD + FPP model)); sections due: 1, 2_

## Status Legend

| Status | Meaning |
|--------|---------|
| 🟢 YES | Item satisfied -- verified by a tool (or attested by the checked record). |
| 🟠 NO | Not satisfied -- a tool found a violation; see Notes. |
| ⚪ N/A | Not applicable -- the model declares nothing this item concerns. |
| 🔵 REVIEW | Needs a human or LLM judgement; the rule to apply is in Notes. |
| ⚫ UNCHECKED | The deciding tool did not run (missing input, tool or license ack); see Notes. |

**Note:** Any status other than **🟢 YES** requires an explanation in the **Notes** column.

## Definitions

| Term | Definition |
|------|------------|
| SDD | Software Description Document — the design documentation for the component. |
| Block diagram | A diagram illustrating the component's ports (and their directions/types), used to visualize the component's interfaces. |

_clang-tidy: 44 rules run; 0 finding(s) in autocoded files were set aside (autocoder, not component)._

## 1. Requirements

| # | Item | Status | Notes |
|---|------|--------|-------|
| 1.1 | Do requirements exist in the component SDD that define the component's expected operational behaviors? | 🟢 YES |  |
| 1.2 | Do requirements exist in the component SDD that define input data expected to be received and handled by the component? | 🟢 YES |  |
| 1.3 | Do requirements exist in the component SDD that define output data expected to be produced by the component? | 🟢 YES |  |
| 1.4 | Do requirements exist in the component SDD for all relevant memory usage constraints for the component? | 🟢 YES |  |
| 1.5 | Do requirements exist in the component SDD for all relevant hardware I/O usage constraints for the component? | 🟢 YES |  |
| 1.6 | Do requirements exist in the component SDD for all relevant runtime performance constraints for the component? | 🟢 YES |  |
| 1.7 | Does every requirement specify a verification method? | 🟢 YES | R8 pass (36 requirement(s) checked) |
| 1.8 | Are there any relevant external interface or functional requirements documented elsewhere such as ICDs/FDDs? If so, please specify in notes. | 🟢 YES | D9 pass (no External References table; cited inline: datasheet 2000) |

## 2. Design

| # | Item | Status | Notes |
|---|------|--------|-------|
| 2.1 | Is the high level component functionality documented/described in the SDD? | 🟢 YES | SDD pass (tier platinum) |
| 2.2 | Is the component block diagram documented in the SDD and consistent with the FPP model? | 🟢 YES | D9 pass (no External References table; cited inline: datasheet 2000) |
| 2.3 | Is the component port table documented in the SDD and consistent with the FPP model? | 🟢 YES | D7 pass (7 port(s) checked) |
| 2.4 | Are the component input port behaviors described in the SDD? | 🟢 YES | D10 pass (4 input port(s) checked) |
| 2.5 | Are the component input port handlers defined? | 🟢 YES | D1 pass (4 input port(s) checked) |
| 2.6 | Are the component command handlers defined? | 🟢 YES | D2 pass (7 command(s) checked) |
| 2.7 | Are the component sequence diagrams documented? | 🟢 YES | D9 pass (no External References table; cited inline: datasheet 2000) |
| 2.8 | Are component state machines and use-cases described in the SDD? | 🟢 YES | D9 pass (no External References table; cited inline: datasheet 2000) |
| 2.9 | Are the state machine actions defined? | ⚪ N/A | D3: no state machines declared |
| 2.10 | Are component commands, telemetry, events and parameters documented in the SDD (and consistent with the FPP model)? | 🟠 NO | D8: SDD command table lists 'Set Exposure', which the FPP model does not declare<br>D8: SDD command table lists 'Set Mode', which the FPP model does not declare<br>D8: event 'FrameDiscarded' is missing from the SDD event table<br>D8: event 'QueueOverflow' is missing from the SDD event table<br>D8: SDD event table lists 'FrameDiscarded, throttle 10', which the FPP model does not declare<br>D8: SDD event table lists 'QueueOverflow, throttle 10', which the FPP model does not declare<br>... 2 more |
| 2.11 | Are component data products documented and consistent with the FPP model? | 🟠 NO | D8: data product 'ImageHeader' is missing from the SDD data product table<br>D8: data product 'ImageLine' is missing from the SDD data product table |
| 2.12 | Are all data product containers assigned a default priority? | 🟢 YES | D4 pass (1 container(s) checked) |
| 2.13 | Does each component port in the FPP model have a known/planned use-case? | 🟢 YES | R6 pass (7 port(s) traced against 36 requirement(s); 7 covered by companion/SDD evidence)<br>D10 pass (4 input port(s) checked) |
| 2.14 | Are port directions and types as expected? | 🟢 YES | D7 pass (7 port(s) checked) |
| 2.15 | Is the component type as expected (passive, queued or active)? | 🟢 YES | D9 pass (no External References table; cited inline: datasheet 2000) |
| 2.16 | Are all the component requirements mapped to design description and/or artifacts? | 🟢 YES | D5 pass (36 requirement(s) checked) |
| 2.17 | Does the component FPP compile and generate auto-code? | 🟢 YES | D6 pass (1 FPP file(s) checked) |

## 3. Implementation

| # | Item | Status | Notes |
|---|------|--------|-------|
| 3.1 | Are all component source dependencies listed in the CMakeLists.txt file for the component? | 🟠 NO | I1: dependency 'Os' (from #include "Os/Mutex.hpp") is not declared in CMakeLists.txt (add it to DEPENDS as Os) |
| 3.2 | Does the component compile without warnings for all required targets? | 🟢 YES | I2 pass (1 log(s) scanned) |
| 3.3 | Are all loops properly terminated/bounded? | 🟢 YES | `cpp/fprime/jpl-c/loop-bounds` 0 findings |
| 3.4 | Has gating been provided for any potential cyclic error/status messages (e.g. log messages, events)? | 🟢 YES | `cpp/fprime/unthrottled-cyclic-event` 0 findings |
| 3.5 | Do all switch statements have a break after each case and terminate with a default case? | 🟢 YES | `cpp/fprime/switch-default` 0 findings<br>`cpp/fprime/switch-case-fallthrough` 0 findings |
| 3.6 | Verify all non-empty case statements that fall through to the following case statement are explicitly commented as such | 🟢 YES | `cpp/fprime/switch-case-fallthrough` 0 findings |
| 3.7 | Are all variables locally defined unless global or specific visibility is required? | 🟢 YES | `cpp/fprime/global-mutable-variable` 0 findings |
| 3.8 | Are all class members defined appropriately using private, public and protected keywords? | 🟢 YES | `cpp/fprime/public-data-member` 0 findings |
| 3.9 | Verify that no floating point expressions are tested for equality/inequality | 🟢 YES | `cpp/fprime/float-equality` 0 findings |
| 3.10 | Is the precision of floating point numbers sufficient to ensure accuracy and floating-point equivalence is evaluated with margin (i.e. safe floating-point usage), and is not used in a loop control? | 🟢 YES | `cpp/fprime/float-narrowing` 0 findings<br>`cpp/fprime/float-loop-control` 0 findings<br>`cpp/fprime/float-equality` 0 findings |
| 3.11 | Are all variables initialized before use? | 🟢 YES | `cpp/fprime/uninitialized-member` 0 findings<br>`cpp/uninitialized-local` 0 findings |
| 3.12 | Are the priorities of all data products produced by the component configurable via command(s) or parameter(s)? | 🟠 NO | I3: container 'Image' priority is not configurable: no command or parameter mentioning 'priority' found |
| 3.13 | Are all commands acknowledged with success/failure? | 🟢 YES | `cpp/fprime/command-response` 0 findings |
| 3.14 | Do all validation failure cases for port and command handlers include a return statement to avoid unintended code execution beyond validation failures? | 🟢 YES | `cpp/fprime/validation-failure-return` 0 findings |
| 3.15 | Are all internal port interfaces invoked via the auto-coded internal port invoke() function? | 🟢 YES | `cpp/fprime/internal-port-invoke` 0 findings |
| 3.16 | All pointer arguments to functions are checked to be not nullptr using FW_ASSERT | 🟠 NO | `cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:682 Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:683 Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:684 Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:768 Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:769 Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:770 Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>... 5 more |
| 3.17 | Assumptions on software state are validated using FW_ASSERT. | 🟠 NO | `cpp/fprime/jpl-c/use-of-assertions-density` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:133 All functions of more than 10 lines should have at least one assertion, unless they handle errors by returning a status enum or bool, or by emitting a WARNING/FATAL event.<br>`cpp/fprime/jpl-c/use-of-assertions-density` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:197 All functions of more than 10 lines should have at least one assertion, unless they handle errors by returning a status enum or bool, or by emitting a WARNING/FATAL event.<br>`cpp/fprime/jpl-c/use-of-assertions-density` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:619 All functions of more than 10 lines should have at least one assertion, unless they handle errors by returning a status enum or bool, or by emitting a WARNING/FATAL event.<br>`cpp/fprime/jpl-c/use-of-assertions-density` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:1014 All functions of more than 10 lines should have at least one assertion, unless they handle errors by returning a status enum or bool, or by emitting a WARNING/FATAL event. |
| 3.18 | Assertions shall not be used to validate state that could be dependent on hardware or user input. | 🟢 YES | `cpp/fprime/assert-on-untrusted-input` 0 findings |
| 3.19 | Verify all ISR code is free of floating point operations | 🟢 YES | `cpp/fprime/no-float-in-isr` 0 findings |
| 3.20 | Has the ISR code been optimized to minimize length/execution time? | 🟢 YES | `cpp/fprime/isr-complexity` 0 findings |
| 3.21 | Verify any software state that's expected upon receive of an interrupt is configured prior to enabling/setting conditions that would allow the interrupt to trigger. | 🔵 REVIEW | CL-1: ISR state before enable (3.21): for every ISR-named function (*_isr, *interrupt_handler*, *_irq*) find the code that enables its interrupt; every variable the ISR reads or writes must be assigned on a path that completes before that enable call. |
| 3.22 | Verify there are no blocking operations in ISRs | 🟢 YES | `cpp/fprime/no-blocking-in-isr` 0 findings |
| 3.23 | Verify any relevant interrupt enables and interrupts are cleared during initialization and re-enabled selectively as needed. | 🔵 REVIEW | CL-2: Interrupt enables cleared at init (3.23): in the constructor/init/configure/preamble, interrupt-enable bits must be explicitly cleared or masked before peripheral configuration, not assumed clear from reset. |
| 3.24 | Are RateGroup schedule calls non-blocking? | 🟢 YES | `cpp/fprime/no-blocking-in-sched-handler` 0 findings |
| 3.25 | Verify no synchronous file access is done within a critical region (e.g. SchedIn) | 🟢 YES | `cpp/fprime/no-file-access-in-critical` 0 findings |
| 3.26 | Verify that all loops and if/else statements are compound statements (i.e. always use brackets {}) | 🟢 YES | `cpp/fprime/compound-statements` 0 findings |
| 3.27 | Verify that all divisions first check for small numbers (including zero) | 🟠 NO | `cpp/fprime/division-guard` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:537 The divisor m_linesPerSlot is not checked against zero before this % operation.<br>`cpp/fprime/float-division-guard` 0 findings |
| 3.28 | Verify all array accesses are checked to be within array index bounds | 🟠 NO | `cpp/fprime/unchecked-array-index` 0 findings<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:682 Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:683 Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:684 Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:768 Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>`cpp/fprime/checking-parameter-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:769 Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>... 6 more |
| 3.29 | Verify there are no function declarations within a function. | 🟢 YES | `cpp/fprime/no-function-decl-in-function` 0 findings |
| 3.30 | Verify that the three expressions in all for statements only concern flow control | 🟢 YES | `cpp/fprime/for-header-flow-control` 0 findings |
| 3.31 | Verify that there are no dependencies on signed integer overflow behavior | 🟢 YES | `cpp/fprime/signed-overflow` 0 findings |
| 3.32 | Verify return values are used or voided otherwise. | 🟠 NO | `cpp/fprime/jpl-c/checking-return-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:115 The return value of non-void function memset is not checked.<br>`cpp/fprime/jpl-c/checking-return-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:116 The return value of non-void function memset is not checked.<br>`cpp/fprime/jpl-c/checking-return-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:117 The return value of non-void function memset is not checked.<br>`cpp/fprime/jpl-c/checking-return-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:118 The return value of non-void function memset is not checked.<br>`cpp/fprime/jpl-c/checking-return-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:119 The return value of non-void function memset is not checked.<br>`cpp/fprime/jpl-c/checking-return-values` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:120 The return value of non-void function memset is not checked.<br>... 5 more |
| 3.33 | Verify units are incorporated into variable name, where applicable | 🔵 REVIEW | CL-3: Units in names (3.33): every variable, member, parameter, telemetry channel and command argument carrying a physical quantity must encode its unit in the name (timeoutMs, angleRad, sizeBytes) or in an FPP annotation. |
| 3.34 | Verify that all invariant numbers are in enums, static constexpr or #defines. Enums or static constexpr must be used unless #define is only way to specify constant. | 🟠 NO | `cpp/fprime/magic-numbers` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:484 Magic number 5; consider a named constant (enum, static constexpr, or #define).<br>`cpp/fprime/magic-numbers` lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:485 Magic number 6; consider a named constant (enum, static constexpr, or #define).<br>`cpp/fprime/define-constant` 0 findings |
| 3.35 | Verify no use of printf | 🟢 YES | `cpp/fprime/no-printf` 0 findings |
| 3.36 | Does the component use platform independent standard data types (e.g. I32, F32 etc.) rather than primitive data types (e.g. int, float etc.)? | 🟢 YES | `cpp/fprime/jpl-c/basic-int-types` 0 findings |
| 3.37 | Does the component pass static analysis? | 🟠 NO | I4: lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:682: [medium] cpp/fprime/checking-parameter-values: Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>I4: lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:683: [medium] cpp/fprime/checking-parameter-values: Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>I4: lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:684: [medium] cpp/fprime/checking-parameter-values: Parameter frame is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>I4: lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:768: [medium] cpp/fprime/checking-parameter-values: Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>I4: lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:769: [medium] cpp/fprime/checking-parameter-values: Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>I4: lib/fprime-devices/fprime-devices/KissCam/Components/SunCamMgr/SunCamMgr.cpp:770: [medium] cpp/fprime/checking-parameter-values: Parameter payload is dereferenced without first being checked (e.g. with FW_ASSERT or a condition).<br>... 17 more |
| 3.38 | Does the implementation address all component requirements and remains consistent with the documented design? | 🔵 REVIEW | CL-4: Implementation matches requirements and design (3.38): for each requirement row locate the handler(s) that satisfy it and confirm the behaviour including each error path the SDD describes; report requirements with no implementing code and handler behaviour the SDD does not describe. |

## 4. Unit Testing

| # | Item | Status | Notes |
|---|------|--------|-------|
| 4.1 | Have all test output/results been published? | ⚫ UNCHECKED | U1 not run: not done yet: no published unit-test artifacts given (--artifacts-dir) |
| 4.2 | Are all component unit test source dependencies listed in the CMakeLists.txt file for the component? | 🟠 NO | U3: unit-test dependency 'Fw/Dp' (from #include "Fw/Dp/DpContainer.hpp") is not declared in the module's CMake files (add it to UT_DEPENDS/DEPENDS) |
| 4.3 | Have input data ranges been tested for boundary conditions? | 🔵 REVIEW | CL-5: Boundary tests (4.3): for each bounded quantity (array sizes, FW_ASSERT limits, configuration constants, command-argument ranges, queue depths) a unit test must exercise the limit value and one past it. |
| 4.4 | Do the unit tests pass a memory leak check? | ⚫ UNCHECKED | U4 not run: not done yet: the leak check (unit-test executable under LeakSanitizer/valgrind) has not been run |
| 4.5 | Is there sufficient code coverage? | ⚫ UNCHECKED | U5 not run: not done yet: no coverage/summary.txt; 'fprime-util check --coverage' has not been run for the module |

## 5. Close-Out

| # | Item | Status | Notes |
|---|------|--------|-------|
| 5.1 | Verify no TBDs, TODOs or printfs in the code (no incomplete code). | 🟢 YES | `cpp/fprime/incomplete-code` 0 findings<br>`cpp/fprime/no-printf` 0 findings |
| 5.2 | Verify that component can be instantiated, connected, and compiled along with the Topology for all applicable targets. | ⚫ UNCHECKED | C1 not run: not done yet: no topology build performed by this run (pass --deployment with --build, or the process builds the deployment) |
| 5.3 | Verify that the flight deployment with component instantiation/connections runs successfully on a hardware platform. Indicate hardware platform used for checkout in Notes. | 🟠 NO | Human: Run the flight deployment that instantiates this component on the target hardware and record platform, deployment, commit, date, result and tester in docs/hardware-run.md (check C2 validates the record).<br>C2: no docs/hardware-run.md: record the hardware run (Platform, Deployment, Commit, Date, Result, Tester) |
| 5.4 | Update the SDD to accurately reflect the final state of the component. Add any additional design detail, including any known idiosyncrasies. | 🟠 NO | D7 pass (7 port(s) checked)<br>D8: SDD command table lists 'Set Exposure', which the FPP model does not declare<br>D8: SDD command table lists 'Set Mode', which the FPP model does not declare<br>D8: event 'FrameDiscarded' is missing from the SDD event table<br>D8: event 'QueueOverflow' is missing from the SDD event table<br>D8: SDD event table lists 'FrameDiscarded, throttle 10', which the FPP model does not declare<br>... 7 more |
| 5.5 | Validate that CI passes. | ⚫ UNCHECKED | C3 not run: not done yet: CI status not consulted (needs the repository, the commit and GITHUB_TOKEN) |

## Hand-off

Nothing to review at the **design** stage.

Deferred to a later stage: 3.21, 3.23, 3.33, 3.38, 4.3.
