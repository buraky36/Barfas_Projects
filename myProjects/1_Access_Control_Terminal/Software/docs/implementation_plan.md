# Access Control Business Logic Fixes

This implementation plan aims to accurately match the `app_state_machine.c` implementation with the `7.Guide_Onloi_Access_ControllerLite-FAZ1` document requirements as discussed.

## Proposed Changes

### `app_state_machine.c`

#### [MODIFY] `app_state_machine.c`
1. **Working Modes (Menu 6) Strict Enforcement:**
   - Modify `hal_wiegand_available()` polling in `app_state_machine_tick()`. If `config.working_mode == 1` (Standalone), Wiegand inputs will be actively ignored to prevent external reader reads.
   - Modify Menu base state logic: if `config.working_mode == 3` (Wiegand Reader), any menu key pressed other than `9` will trigger a beep error and be ignored.

2. **Access Mode Verification (Menu 5):**
   - Update `process_auth_pass_command` to evaluate `config.access_mode` before granting access.
   - If the incoming prefix (e.g., `RFID`, `KEYPAD`, `QR`) violates the current `access_mode` (e.g., trying KEYPAD when mode is 0: Card Only), access will be denied and error beeps will play.

3. **User IDLE PIN Change Sequence:**
   - In `STATE_IDLE`, adjust keyboard handling so that pressing `*` does *not* immediately commit to entering the Master Code unless a specific timeout or logic completes.
   - Actually, a better approach: when `*` is pressed, enter a `STATE_WAIT_FOR_MASTER_OR_USER` where it buffers input. If the input ends with `#`, it checks if it matches the Master Code. If it does, it enters Programming Mode. If it is NOT the Master Code, it attempts to parse it as a `UserID` and start the `OldPIN -> # -> NewPIN -> # -> Repeat NewPIN -> #` sequence.

4. **Program Mode Exit with `*`:**
   - Ensure that inside `STATE_PROGRAMMING` and all `PROG_STATE_*` sub-states, pressing the `*` key immediately flushes the buffer, plays the exit beeps, and returns to `STATE_IDLE`.

5. **Relay Toggle Mode (Menu 4):**
   - In `STATE_DOOR_OPEN`, if `config.relay_time == 0`, the timer check will be bypassed (door stays open indefinitely).
   - Upon a successful authentication, check if the relay is currently ON and `config.relay_time == 0`. If so, turn the relay OFF and return to `STATE_IDLE`. If OFF, turn it ON and enter `STATE_DOOR_OPEN`.

6. **Open Door Detection (Menu 7) Fix:**
   - Change `PROG_STATE_DOOR_DETECTION` to expect a 2-step sequence.
   - `input == 3`: Disable door detection.
   - `input == 4`: Go to sub-state to read `0~3` minutes.

7. **Menu 2 & 3 Enhancements (Users):**
   - **Menu 2 (Add User)**: Refactor buffer sequence to support `2 -> Read Card -> #` (Assign auto-increment ID), `2 -> UserID -> # -> PIN -> #`, and `2 -> CardNumber -> #` to correctly route between PIN assignments and Card assignments.
   - **Menu 3 (Delete User)**: Add a check if the input matches `config.master_code`. If so, trigger `nv_storage_delete_all_users()`.

## Open Questions

> [!IMPORTANT]
> **User IDLE PIN Change Protocol Compatibility:**
> Allowing users to change their PIN requires us to accept unstructured input starting with `*` and then look up the User. The previous document mentioned starting Master Code sequence with `* -> MasterCode -> #`. We will use this logic: When the user types `* -> XXXXX -> #`, we first check if `XXXXX` is the Master Code. If it is, we enter Setup. If it is NOT the Master Code, we assume `XXXXX` is a `UserID` and prompt for their Old PIN. Does this logic align with your expectations?

## Verification Plan

### Manual Verification
1. Flash to the development kit and connect TSM12, XR1300, MFRC522.
2. In Mod 1, verify Wiegand reader does not work.
3. Test Access Modes (e.g., set to Card Only, try authenticating with PIN -> should fail).
4. In Menus 2 and 3, verify we can add specific PINs and delete all users.
5. In Menu 4, test Toggle Mode (`4 -> 0 -> #`), verifying the relay toggles state recursively upon valid reads.
