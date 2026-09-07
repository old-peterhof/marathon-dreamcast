# Good Boy Gumball

A Game Boy (DMG) game about a dog who is worried he was bad.

---

## 1. The premise

Gumball wakes on a grassy field beside a patch of fresh-turned soil. The field is
overgrown, wild, unkept. His family is not here. He remembers, in flashes, the
things he did wrong: he bit the boy, he stole from the girl's plate, he dug up
the mother's garden, he barked all night and ran when the father called.

He goes to find them. He is afraid that when he does, they will not say he was a
good boy.

He finds them one at a time. Each one says it.

---

## 2. What the game is actually about, structurally

Guilt is a strong engine for a platformer because guilt makes you *move*. Gumball
is not exploring and he is not collecting. He is atoning, and that gives every
level a reason to exist that "get to the flag" does not.

The reveal at the end works only if it recontextualizes the **verbs**, not just
the story. So the central rule of the design:

> **Every world is built around the exact sin Gumball committed against that
> family member. The mechanic you are ashamed of is the mechanic that saves them.**

Put the bad memory in the controller, not in a cutscene.

And the second rule, for the bosses:

> **The sins are what Gumball did. The bosses are what Gumball was afraid of.**

Every boss is a real dog fear, built out of that world's material, and beaten
using that world's verb. Nothing in this game is a monster. Everything in it is
either a household object or the weather.

---

## 3. The four worlds

Order runs from child-scale sins to the one that hurts most, and the last world
sits closest to how he probably died without ever saying so.

### World 1 — Foon (the Boy)
**Sin:** he bit him, playing too rough.
**Verb: BITE.** Attack and traversal both. Bite a rope and swing, bite a toy and
carry it, bite a sheet and pull it down into a bridge.
**Theme:** the house at a kid's height. Toy blocks, a cushion fort, a hallway
that is a mile long, the back yard through a screen door.

| Level | Setting | Beat |
|---|---|---|
| 1-1 | Foon's bedroom | Learn to bite. Everything soft. |
| 1-2 | Hallway and stairs | Bite-swing. Cat appears. Memory: the bite. |
| 1-3 | The bathroom | **Boss: THE BATH.** |

**Forgiveness:** "You bit me because we were playing. You were playing *with* me."

### World 2 — Loba (the Girl)
**Sin:** he stole food off her plate.
**Verb: CARRY (and resist).** You carry food you are not allowed to eat. A
hunger meter climbs. Eating what you carry heals you *and* fails the objective.
**Theme:** the kitchen and dining room, scaled so Gumball is small. Counters as
platforms, chair legs as pillars, the underside of the table as a ceiling.

| Level | Setting | Beat |
|---|---|---|
| 2-1 | The pantry | Learn to carry. Nothing is chasing you yet. |
| 2-2 | Countertops and sink | Carry across gaps. Cat appears. Memory: the plate. |
| 2-3 | Under the dining table | **Boss: THE VACUUM.** |

**Forgiveness:** "I always gave you some anyway. I just liked when you sat with me."

### World 3 — Chelsea (the Mother)
**Sin:** he dug up her garden and tracked mud through the house.
**Verb: DIG.** Destructive and also the only way forward. Buried paths, buried
keys, buried carrots.
**Theme:** the garden gone feral. Overgrown beds, a greenhouse with cracked
panes, a compost heap. This world visually rhymes with the opening field.

| Level | Setting | Beat |
|---|---|---|
| 3-1 | The flower beds | Learn to dig. Soft soil everywhere. |
| 3-2 | The greenhouse | Dig under glass. Cat appears. Memory: the ruined bed. |
| 3-3 | The long grass | **Boss: THE LAWNMOWER.** |

**Forgiveness:** "I replanted it every spring. I liked having a reason to."

### World 4 — Chewie (the Father)
**Sin:** he barked all night, and he ran when called, and he did not come back.
**Verb: BARK, and the LEASH.** Bark to call things toward you and to find your
way. A leash tether limits your distance from an anchor. In the last stretch of
the last level the leash comes off.
**Theme:** the neighborhood at night. Streetlights, driveways, a road. The only
world with real quiet in it, and the only one with a dark palette.

| Level | Setting | Beat |
|---|---|---|
| 4-1 | The back fence and alley | Learn to bark. Learn the leash. |
| 4-2 | Driveways and parked cars | Bark to navigate. Cat's last appearance. |
| 4-3 | The street, rain starting | **Boss: THE STORM.** |

**Forgiveness:** "I wasn't angry. I was scared. I'm not scared now."

---

## 4. The three-level shape

Every world uses the same three-beat template. The player learns the shape once
and then the anticipation does half the work.

- **Level 1 — Teach.** Introduce the verb in a safe space. No fear pressure, no
  cat, no failure state that costs much. End it somewhere warm.
- **Level 2 — Complicate.** The verb gets a second use and a cost. The black cat
  appears here, once. One playable flashback: a short, muted, small-arena scene
  of the actual bad memory, in which the player commits the sin themselves and
  cannot avoid it.
- **Level 3 — Fear.** The world's fear starts leaking into the level before the
  boss arena: distant mower noise, water on the tiles, a vacuum cord across the
  floor, thunder with no rain yet. Boss at the end. Reunion after.

The flashback in level 2 is important. Do not let the player merely *hear* that
he bit Foon. Make them press the button.

---

## 5. The bosses

None of them are alive. Three are machines and one is the sky. All of them are
three phases, one hit per phase, because the reward is the reunion and not the
challenge.

### 1-3 — THE BATH
The tub is filling and will not stop. Suds churn, the shower head sweeps the
room, the tiles are slick.

You cannot bite water. That is the joke and that is the puzzle. Instead you bite
the plug chain, you bite the towel down off the rail to make a raft, you bite the
curtain rod so it falls across the tub. Three bites, three phases, the water
drains.

The lightest boss in the game, deliberately. World 1 should be the one that makes
someone laugh.

### 2-3 — THE VACUUM
It is under the dining table with you and it wants the crumbs. It inhales
carrots off the floor, which means it is stealing your health while you are
carrying food you are not allowed to eat.

**The boss is greedier than Gumball ever was.** That is the entire point of it.

You beat it by carrying heavy things into its intake: a bone, a wooden block, a
napkin ring. Three jams and it chokes. Everything it swallowed spills back out.

### 3-3 — THE LAWNMOWER
It cuts lanes through the tall grass you are hiding in, so the arena shrinks
every pass. It shreds the flower beds as it goes.

**It is doing to the garden exactly what Gumball did, only worse and on purpose.**
He dug up her flowers; this thing erases them. He has to dig to save what is
left.

You dig trenches it stalls in and you dig up stones that jam the blade. Three
jams. When it dies the grass stays cut, and the path to Chelsea is the lane it
carved.

### 4-3 — THE STORM
You cannot hit weather. Lightning telegraphs and then deletes a platform. Wind
pushes. The rain gets heavier for the whole fight.

The first time you bark, nothing answers. Silence. This is the only moment in the
game where the verb fails, and it should sit for a full two seconds.

Then you learn: bark from the right places. A parked car, a mailbox, a porch
light. Each answered bark is Chewie's voice, closer than the last. Three anchors,
three answers.

On the third, he calls Gumball's name from across the street. The leash comes
off. You run to him, into the road, and the screen whites out on a thunderclap.

Then you are in his arms.

That is the emotional peak of the game and it needs no explanation, no dialogue
box, and absolutely no follow-up joke.

---

## 6. The reunion

The most important twelve seconds in each world, and it is the same ritual every
time. Repetition is what makes it land.

**Beat 1.** The boss stops. All music cuts. Not a fade, a cut. Silence and room
tone.

**Beat 2.** The family member is standing there, doing something completely
ordinary. Foon holding a ball. Loba holding a plate. Chelsea with a trowel.
Chewie holding a leash with nothing on the end of it.

**Beat 3.** The player has control. There is no prompt, no arrow, no button
hint. You have to walk to them yourself. Most players will hesitate, and the
hesitation is the design.

> If the player stands still, the family member waits. Forever. They never leave
> and they never call out. That is a statement and it costs you nothing to
> implement.

**Beat 4.** Within about three tiles, control is taken for the **shy-away**:
Gumball stops, ears flatten, head turns aside, tail tucks under. He is bracing to
be scolded. Roughly one second in world 1.

**Beat 5.** The hand comes down. He flinches into it. Pet, then hug, then the
line. Then:

- max carrots +1, and the bar visibly grows
- the idle sprite upgrades a stage
- a short "good boy" motif plays, different for each family member
- **the world theme never plays again**

### The shy-away gets shorter every time
World 1 it is a full second and he almost backs up. World 2 he stops. World 3 he
only ducks his head. World 4 it is a single frame of hesitation and then he is
already running.

That is the whole character arc rendered in animation frames, and it costs four
short sprite sequences.

### The cat goes before, not after
The black cat's line lands immediately *before* each boss, never after the
reunion. Whatever it says is what the player carries into the fight. The
forgiveness is the answer to it and nobody has to point that out.

---

## 7. The black cat

Appears twice per world: once in level 2, once at the door to the boss. Each
appearance is shorter than the one before it.

Hard rules:

1. **It never attacks.** It is never a boss. There is no fight.
2. **It never lies.** Everything it says is true. "You did bite him. I was
   there." The cruelty is entirely in the framing. Far more unsettling than a
   liar and much harder to argue with.
3. **It helps you.** It knows where the family members are. It tells you. It is
   not withholding assistance, it is charging you emotionally for assistance you
   need.
4. **It is not defeated.** In the finale it is simply present, and the family
   knows it. Optional and devastating: the cat is also dead, and is also waiting
   for someone, and that someone has not come.
5. **Never confirm whether it is a real cat or Gumball's own guilt.** Both
   readings must survive to the credits.

Sample lines, for tone:

> "They buried you in a nice spot."
> "Do you think they wanted you to find them?"
> "The boy still has the scar. I'm only saying what's true."
> "You are looking for people who had to learn to live without you."
> "Go on, then. He's just through there." *(before a boss)*

---

## 8. Carrots

Carrots are health, because Gumball loved them.

- The health bar is a row of carrot icons, not hearts.
- A small carrot restores one.
- Hidden golden carrots exist per level; three in a world unlocks something small
  and warm, not a power.
- **Every forgiveness permanently raises max carrots by one.** Love is literally
  the health bar. Four family members, four extra carrots. Nearly free to
  implement, and it means the player is measurably stronger for having been
  forgiven.

---

## 9. The idle sprite

The single highest-value cheap idea in the whole design.

Gumball's idle animation starts with his tail down, ears flat, head low. After
each forgiveness the idle changes: ears up, head lifts, tail starts to move, and
after the fourth it wags.

A handful of tiles. No dialogue. Every player will notice it and no player needs
it explained. It is the same trick as the shrinking shy-away and they reinforce
each other.

---

## 10. The opening and the ending

**Opening.** He wakes on the field. The soil patch is fresh-turned but the grass
around it is wild and tall. This is his grave. **Never explain it.** Sharp players
understand within thirty seconds; everyone else gets there at the end. Both
experiences are good.

**Ending.** No final boss. The last map is a walk back across the field. One at a
time each family member falls in behind you until all four are walking with you.
The four "good boy" motifs layer in as they join, so the finale theme assembles
itself out of the four reunions. The patch of soil is gone, covered in flowers.
The last line is the title of the game. Fade.

Do not add a post-credits stinger. Do not add a second ending. Let it be over.

---

## 11. Game Boy technical notes

Target: DMG (original Game Boy), MBC5, 512 KB. Runs on GBC and on Super Game Boy
without needing to.

### Sprites
- 160x144 screen, 4 shades, 8x8 background tiles.
- Hardware limit: **40 sprites total, 10 per scanline.** The per-scanline limit is
  the one that will actually bite you.
- Use **8x16 sprite mode** (LCDC bit 2). Almost every commercial DMG game does. A
  16x16 Gumball is 2 hardware sprites instead of 4.
- Budget: Gumball 2-4 sprites, at most 3 enemies at 2-4 each, and keep a reserve
  for pickups. Design so vertical stacking of enemies is rare.

### Bosses on DMG
This is the part that will humble you if you plan it late.

- **Draw the boss body in the background layer, not with sprites.** A big object
  made of sprites blows the 10-per-scanline limit instantly. Animate it by
  rewriting BG tiles during VBlank and by scrolling.
- Use sprites only for the small moving parts: the shower head, the vacuum
  nozzle, the mower blade, the lightning flash.
- The storm is the cheapest of the four: lightning is a **palette flash**
  (write BGP for a few frames), rain is a scrolling 1bpp overlay, and the wind is
  a constant added to Gumball's velocity. Almost no tile budget.
- Each boss gets its own tileset, swapped in when level 3 loads. One unique boss
  tileset per world fits comfortably in 512 KB.
- Three phases, one hit each. Do not build a health bar for them.

### Banking
- One bank per world tileset, one per boss.
- Dialogue text gets its own banks and is cheap. This is a dialogue-heavy game
  and that is good news on this hardware.

### Build the text engine first
The emotional payload is text. A variable-width font, a typewriter reveal, a
portrait window, and a simple script format are the *first* thing to build, not
the last. If the text engine is bad the game is bad, no matter how the
platforming feels.

### Audio
Four channels. Write **one** Gumball melody. Each world theme is a variation of
it: Foon bright and fast, Loba playful, Chelsea slow and warm, Chewie sparse and
low. Each "good boy" motif is four or five notes from that melody. The finale is
the full arrangement, the first time you hear it whole.

### Assembly vs C: an honest recommendation

You have a book on Game Boy assembly. Which path depends on what you want.

- **If the goal is learning the hardware:** write it in RGBDS assembly. Twelve
  levels and four bosses is a lot for a first Game Boy project, so be willing to
  cut world 2 or 3 down to two levels if month four arrives and you are not
  halfway.
- **If the goal is finishing the memorial:** use GBDK-2020 (C) for game logic and
  hand-write only the renderer, the scroll, and the collision in ASM. Several
  times faster, and the parts that need to be tight still will be.

Either way, prototype **1-1 and the bite mechanic** before anything else. If
biting is not fun the whole structure needs rethinking, and you want to know that
in week two rather than month four. Prototype **the reunion** second, with
programmer art, because it is the thing the entire game is for.

---

## 12. Scope

| Piece | Count |
|---|---|
| Opening field | 1 map |
| Worlds | 4 |
| Levels per world | 3 |
| World levels | 12 maps |
| Finale walk | 1 map |
| **Total playable maps** | **14** |
| Bosses | 4 |
| Playable flashbacks | 4 (one per world, in level 2) |
| Reunions | 4 |
| Music tracks | 6 themes + 4 "good boy" motifs |
| Black cat scenes | 8 |

Ship a vertical slice of World 1 first, all three levels and the bath and the
reunion. Everything after that is a repeat of a proven shape with different art.

---

## 13. Risks worth naming

1. **The first 80% has to be fun without the twist.** Guilt is not enjoyable for
   three hours on its own. The verbs carry it, which is why each world gets a
   distinct mechanic rather than a distinct coat of paint.
2. **Four bosses is where a first Game Boy project dies.** Build the bath as a
   background-layer object early and prove the technique before designing the
   other three around it.
3. **The cat can become preachy.** Two appearances per world, hard cap, each
   shorter than the last. If a line does not make the player wince, cut it.
4. **Sentiment can outrun the game.** Resist having characters state the theme.
   The idle sprite, the shrinking shy-away, the growing carrot bar, and the
   flowers over the soil say all of it without a word.
5. **Do not let the player fail a forgiveness.** There is no version of this game
   where a family member withholds it, and no version where a boss can kill you
   during the reunion walk. The tension is Gumball's fear, not an actual risk.

---

*For Gumball. Good boy.*
