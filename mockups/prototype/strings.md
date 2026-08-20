# Interface text

Every word the player can read. Edit the text under each `###` key, then run:

    python3 build.py && python3 check.py

`build.py` reads this file, so there is nothing to paste back. Rules:

- **Do not change the `###` keys.** `build.py` fails loudly on a missing one.
- Keep one item per line where a key holds a list.
- `{name}`, `{level}`, `{time}`, `{free}`, `{n}` are filled in at runtime. Leave
  them spelled exactly like that.
- ALL CAPS below is literal — the design does not uppercase anything for you.
- Room is tight. Check the width of anything you lengthen; `check.py` will catch
  text that overflows its box, but not text that merely looks cramped.

---

## Shared

### build.label
b66

## Shared — button hints

### hint.select
SELECT

### hint.move
MOVE

### hint.adjust
ADJUST

### hint.open
OPEN

### hint.back
BACK

### hint.begin
BEGIN

### hint.load
LOAD

### hint.delete
DELETE

### hint.bind
BIND

### hint.defaults
DEFAULTS

### hint.cancel
CANCEL

### hint.resume
RESUME

### hint.next
NEXT

### hint.pause
PAUSE

---

## Main menu

### main.cap
MAIN

### main.item.new
NEW GAME

### main.item.continue
CONTINUE GAME

### main.item.saves
MANAGE SAVES

### main.item.prefs
PREFERENCES

### main.item.credits
CREDITS

### main.state.label
LAST SAVE

---

## New game

### difficulty.title
NEW GAME

### difficulty.kicker
SELECT DIFFICULTY

### difficulty.cap
DIFFICULTY

### difficulty.names
KINDERGARTEN
EASY
NORMAL
MAJOR DAMAGE
TOTAL CARNAGE

---

## Manage saves

### saves.title
MANAGE SAVES

### saves.kicker
VMU A1 · {free} OF 200 BLOCKS FREE

### saves.cap
SAVED GAMES

### saves.cap.right
4 SLOTS

### saves.empty
EMPTY SLOT

### saves.banner
ALL FOUR SLOTS FULL · CHOOSE ONE TO OVERWRITE

---

## Preferences — root

### prefs.title
PREFERENCES

### prefs.kicker
SAVED TO VMU

### prefs.brightness
BRIGHTNESS

### prefs.brightness.note
How bright the picture is. Have fun.

### prefs.brightness.values
DARKEST
DARKER
DARK
NORMAL
LIGHT
REALLY LIGHT
EVEN LIGHTER
LIGHTEST

### prefs.sound
SOUND

### prefs.sound.note
Volume, quality, and extras.

### prefs.controls
CONTROLS

### prefs.controls.note
Stick behaviour, sensitivity, and controller config.

---

## Preferences — sound

### sound.title
SOUND

### sound.kicker
PREFERENCES

### sound.volume
VOLUME

### sound.volume.note
Sound effects and music together.

### sound.quality
QUALITY

### sound.quality.note
8-bit uses half the memory but sounds worse. Applies next launch.

### sound.quality.on
16 BIT

### sound.quality.off
8 BIT

### sound.stereo
STEREO

### sound.stereo.note
Turn this off if the TV has one speaker. Applies next launch.

### sound.panning
ACTIVE PANNING

### sound.panning.note
Sounds move between the speakers as you turn.

### sound.ambient
AMBIENT SOUNDS

### sound.ambient.note
Fans, water and machinery running in the background.

### sound.more
MORE SOUNDS

### sound.more.note
Awesome but slower load times.

---

## Preferences — controls

### controls.title
CONTROLS

### controls.kicker
PREFERENCES

### controls.stick
ANALOG STICK

### controls.stick.note
Look is mouselook. Move walks and turns.

### controls.stick.values
LOOK
MOVE

### controls.turnsens
TURN SENSITIVITY

### controls.turnsens.note
How fast the stick turns you.

### controls.looksens
LOOK SENSITIVITY

### controls.looksens.note
How fast the stick looks up and down.

### controls.invert
INVERT LOOK

### controls.invert.note
Push the stick forward to look down.

### controls.run
ALWAYS RUN

### controls.run.note
Run without holding Run/Swim.

### controls.swim
ALWAYS SWIM

### controls.swim.note
The same, for swimming.

### controls.autoswitch
AUTO-SWITCH WEAPONS

### controls.autoswitch.note
Draw a new weapon the moment you pick it up.

### controls.configure
CONFIGURE CONTROLLER

### controls.configure.note
Self-explanatory.

---

## Configure controller

### controller.title
CONFIGURE CONTROLLER

### controller.kicker
20 ACTIONS

### controller.cap.main
ACTIONS

### controller.cap.adv
ADVANCED

### controller.spent
{n} OF 10 BUTTONS SPENT

### controller.page.toadv
ADVANCED

### controller.page.tomain
ACTIONS

### controller.bind.note
Press A, then press the button you want for {name}.

### controller.capture.title
BINDING

### controller.capture.body
Press a button for {name}.

### controller.capture.foot
ANY BUTTON BINDS · START CANCELS

### controller.actions.main
<!-- Order matters: these lines pair with the default bindings in build.py by
     position. Renaming a line is safe. Reordering or inserting one is not. -->
MOVE FORWARD
MOVE BACKWARD
SIDESTEP LEFT
SIDESTEP RIGHT
LOOK UP
LOOK DOWN
LOOK AHEAD
PREVIOUS WEAPON
NEXT WEAPON
TRIGGER
ALT TRIGGER
ACTION
MAP

### controller.actions.adv
<!-- Order matters here too — see the note above. -->
TURN LEFT
TURN RIGHT
GLANCE LEFT
GLANCE RIGHT
SIDESTEP
RUN/SWIM
LOOK

---

## Pause

### pause.title
PAUSED

### pause.cap
GAME

### pause.item.resume
RESUME

### pause.item.save
SAVE GAME

### pause.item.prefs
PREFERENCES

### pause.item.quit
QUIT TO MAIN MENU

### pause.state.elapsed
ELAPSED

### pause.state.lastsave
LAST SAVE

---

## Confirmations

### save.title
SAVED

### save.note
VMU. {free} of 4 slots free.

### save.yes
CONTINUE

### overwrite.title
OVERWRITE SLOT

### overwrite.body
Replace “{name}”?

### overwrite.note
All four slots are full. This one becomes “{level}” at {time}.

### overwrite.no
KEEP

### overwrite.yes
OVERWRITE

### delete.title
DELETE SAVE

### delete.body
Delete “{name}”?

### delete.note
You cannot undo this.

### delete.no
KEEP

### delete.yes
DELETE

### quit.title
QUIT TO MAIN MENU

### quit.body
Leave the current game?

### quit.note
You lose everything since your last save.

### quit.no
STAY

### quit.yes
QUIT

---

## Credits

### credits.title
CREDITS

### credits.roll
MARATHON 2: DURANDAL
= Bungie Software Products Corporation
= 1995
DESIGN
= Jason Jones
= Greg Kirkpatrick
ENGINEERING
= Jason Jones
= Ryan Martell
ART
= Craig Mullins
= Mark Bernal
= Robert McLees
SOUND
= Alexander Seropian
= Paul Heitsch
ALEPH ONE
= The Aleph One contributors
= Open source since 2000
DREAMCAST PORT
= Built on KallistiOS
= b56

---

## Sample data

Placeholder content, not copy. Level names are Bungie's.

### sample.level
CHARON DOESN'T MAKE CHANGE

### sample.elapsed
02:58:04

### sample.lastsave
41 MINUTES AGO

### sample.saves
WAITING PERIOD | 03:41:12 | A1 12 BLK
CHARON DOESN'T MAKE CHANGE | 02:58:04 | A1 12 BLK
COME AND TAKE YOUR MEDICINE | 02:12:47 | A1 12 BLK
—

