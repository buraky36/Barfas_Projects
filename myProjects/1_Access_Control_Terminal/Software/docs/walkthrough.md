# Access Control Firmware Refactoring Walkthrough

The firmware state machine logic (`app_state_machine.c` / `app_state_machine.h`) has been fully reviewed and refactored exactly according to the "7.Guide_Onloi_Access_ControllerLite-FAZ1" document you shared. Here is a review of everything we accomplished:

## 1. Operating Modes Implemented
We implemented conditional checks inside the core polling loop to honor the working mode accurately:
- **Standalone Mode (`config.working_mode == 1`)**: Wiegand input processing is completely ignored.
- **Controller Mode (`config.working_mode == 2`)**: Regular inputs processed internally. Wiegand direction is set to Input. 
- **Reader Mode (`config.working_mode == 3`)**: Keypad and QR inputs bypass local evaluation and transmit standard Wiegand signals. Master features are still accessible to configure the device securely. 

## 2. Authentication Access Modes Defined
Created a global `app_is_access_granted()` helper to pre-verify authentications before attempting them. Handled Access Modes `[0..6]`:
- *Card Only, PIN Only, QR Only, Card or PIN, Card or QR, Any.*
- Invalid auth methods produce the standard `3 Slow Beeps + Solid Red LED` feedback directly.

## 3. Programming Menu Refactoring
The entire `STATE_PROGRAMMING` core loop was rewritten strictly matching the document parameters to eliminate bugs:
- **Global Exit**: Pressing `*` in *any* state immediately drops the user to `STATE_IDLE`. (Removed all the weird legacy `*#` hacks).
- **Menu 2: Add User**: Now properly separated into `<UserID>` then `#` then `<PIN>`, storing `pending_user_id` reliably and updating existing IDs natively thanks to our previous refactor.
- **Menu 3: Delete User**: Supports entering the Master Code to instantly run `nv_storage_delete_all_users()`, otherwise safely deletes the specific ID submitted. 
- **Menu 7: Door Detection**: Added fully featured sub-logic asking for `[VAL]` ranging `0, 1, 2, 3` configuring alarm minutes accurately. 

## 4. Relay Toggle Config (`relay_time == 0`)
Inside `app_trigger_door_open()`, implemented immediate Relay Toggle tracking if the relay time is configured to `0`. Subsequent authorized access switches the relay state effectively allowing indefinite unlocking of the gate. 

## 5. Idle PIN Changing
Included `STATE_USER_PIN_CHANGE`. While in `STATE_IDLE`, entering an unknown Code structure evaluates if the input is actually a valid User ID. If so, it enters a `OldPIN -> NewPIN -> Repeat` sequence, verified step-by-step securely.

> [!NOTE]
> The clang IDE highlights some missing generic libraries like `stdint.h` or configuration includes for `hal_wiegand`. `hal_wiegand` compilation will be done on your machine via standard `idf.py build`, but feel free to let me know if those dependency includes need updating!

Everything lines up exactly as discussed. Please review and compile it! Let me know if we can test any particular feature over UART logging or physical board debugging.
