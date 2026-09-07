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

---

## 3. The four worlds

Order runs from child-scale sins to the one that hurts most, and the last world
sits closest to how he probably died without ever saying so.

### World 1 — Foon (the Boy)
**The sin:** he bit him, playing too rough.
**Theme:** the boy's room and the back yard. Toy blocks, a fort of couch
cushions, a fence, a hose. Bright, cluttered, small-scale.
**Verb: BITE.** Bite is your attack, but it is also traversal: bite a rope and
swing, bite a toy and carry it, bite a sheet and pull it down to make a bridge.
The game forces you to use the thing he is ashamed of over and over.
**The forgiveness:** "You bit me because we were playing. You were playing
*with* me."

### World 2 — Loba (the Girl)
**The sin:** he stole food off her plate.
**Theme:** the kitchen and the pantry, scaled up so Gumball is small. Counters
as platforms, a sink, cereal boxes, a running dishwasher.
**Verb: CARRY (and resist).** You carry food you are not allowed to eat. A
hunger meter climbs. Eating what you carry restores health *and* fails the
objective. The temptation is the level design.
**The forgiveness:** "I always gave you some anyway. I just liked when you sat
with me."

### World 3 — Chelsea (the Mother)
**The sin:** he dug up her garden and tracked mud through the house.
**Theme:** the garden, gone feral. Overgrown beds, a greenhouse with cracked
panes, a compost heap. This world visually rhymes with the opening field.
**Verb: DIG.** Digging is destructive and it is also the only way forward.
Buried paths, buried keys, buried carrots. You wreck the garden to save her.
**The forgiveness:** "I replanted it every spring. I liked having a reason to."

### World 4 — Chewie (the Father)
**The sin:** he barked all night, and he ran off when called, and he did not come
back.
**Theme:** the neighborhood at night. Streetlights, driveways, a road. Dark
palette, long sightlines, the only world with real quiet in it.
**Verb: BARK, and the LEASH.** Bark to solve puzzles and call things toward you.
A leash tether limits how far you can go from an anchor point; the last stretch
of the last level, the leash comes off and you have to run.
**The forgiveness:** "I wasn't angry. I was scared. I'm not scared now."

---

## 4. The black cat

A recurring figure. It appears twice per world, and each appearance is shorter
than the one before it.

Hard rules:

1. **It never attacks.** It is never a boss. There is no fight.
2. **It never lies.** Everything it says is true. "You did bite him. I was
   there." The cruelty is entirely in the framing. This is far more unsettling
   than a liar and much harder to argue with.
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

---

## 5. Carrots

Carrots are health, because Gumball loved them.

- The health bar is a row of carrot icons, not hearts.
- A small carrot restores one.
- Hidden golden carrots exist per level; three in a world unlocks something
  small and warm, not a power.
- **Every forgiveness permanently raises max carrots by one.** Love is literally
  the health bar. Four family members, four extra carrots. It costs almost
  nothing to implement and it means the player is measurably stronger for having
  been forgiven.

---

## 6. The idle sprite

The single highest-value cheap idea in the whole design.

Gumball's idle animation starts with his tail down, ears flat, head low. After
each forgiveness, the idle changes: ears come up, head lifts, tail starts to
move, and after the fourth it wags.

A handful of tiles. No dialogue. Every player will notice it and no player needs
it explained.

---

## 7. The opening and the ending

**Opening.** He wakes on the field. The soil patch is fresh-turned but the grass
around it is wild and tall. This is his grave. **Never explain it.** Sharp
players understand within thirty seconds; everyone else gets there at the end.
Both experiences are good.

**Ending.** No final boss. The last level is a walk back across the field. One at
a time each family member falls in behind you until all four are walking with
you. The patch of soil is gone, covered in flowers. The last line is the title of
the game. Fade.

Do not add a post-credits stinger. Do not add a second ending. Let it be over.

---

## 8. Game Boy technical notes

Target: DMG (original Game Boy), MBC5, 512 KB. Runs on GBC and on Super Game Boy
without needing to.

### Sprites
- 160x144 screen, 4 shades, 8x8 background tiles.
- Hardware limit: **40 sprites total, 10 per scanline.** The per-scanline limit is
  the one that will actually bite you.
- Use **8x16 sprite mode** (LCDC bit 2). Almost every commercial DMG game does.
  A 16x16 Gumball is 2 hardware sprites instead of 4.
- Budget: Gumball 2-4 sprites, at most 3 enemies on screen at 2-4 sprites each,
  and keep a reserve for pickups. Design levels so vertical stacking of enemies
  is rare.

### Banking
- One bank per world tileset. Four worlds is a comfortable fit.
- Dialogue text gets its own banks and is cheap. This is a dialogue-heavy game and
  that is good news on this hardware.

### Build the text engine first
The emotional payload of this game is text. A variable-width font, a typewriter
reveal, a portrait window, and a simple script format are the *first* thing to
build, not the last. If the text engine is bad, the game is bad, no matter how
the platforming feels.

### Audio
Four channels. Write **one** Gumball melody. Each family member's world theme is
a variation of it in a different mood: Foon bright and fast, Loba playful,
Chelsea slow and warm, Chewie sparse and low. The finale is the full
arrangement, first time you hear it whole. This is an old trick and it works
every time.

### Assembly vs C: an honest recommendation

You have a book on Game Boy assembly. Which path depends on what you want.

- **If the goal is learning the hardware:** write it in RGBDS assembly and cut
  the scope hard. Four worlds of two levels each, plus the opening field and the
  finale. Ten playable maps total. That is a real, finishable first Game Boy
  game and you will genuinely know the machine at the end of it.
- **If the goal is finishing the memorial:** use GBDK-2020 (C) for the game logic
  and hand-write only the renderer, the scroll, and the collision in ASM. You
  will get there several times faster and the parts that need to be tight will
  still be tight.

Either way, prototype **World 1, Level 1 and the bite mechanic** before anything
else. If biting is not fun, the whole structure needs rethinking, and you want
to know that in week two rather than month four.

---

## 9. Scope

Recommended minimum viable version:

| Piece | Count |
|---|---|
| Opening field | 1 map |
| Worlds | 4 |
| Levels per world | 2 + 1 set piece |
| Finale walk | 1 map |
| Total playable maps | 14 |
| Music tracks | 6 (one per world, opening, finale) |
| Black cat scenes | 8 |

Ship a vertical slice of World 1 first. Everything else is a repeat of a proven
shape.

---

## 10. Risks worth naming

1. **The first 80% has to be fun without the twist.** Guilt is not enjoyable for
   three hours on its own. The verbs have to carry it. This is why each world
   gets a distinct mechanic rather than a distinct coat of paint.
2. **The cat can become preachy.** Two appearances per world, hard cap, each one
   shorter than the last. If a line does not make the player wince, cut it.
3. **Sentiment can outrun the game.** Resist the urge to have characters state
   the theme. The idle sprite, the growing carrot bar, and the flowers over the
   soil say all of it without a word.
4. **Do not let the player fail a forgiveness.** There is no version of this
   game where a family member withholds it. The tension is Gumball's fear, not
   an actual risk.

---

*For Gumball. Good boy.*
