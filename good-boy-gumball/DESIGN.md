# Good Boy Gumball

A Game Boy (DMG) game about a dog who is worried he was bad.

---

## How to read this document

This is a memorial for a real dog and a real family. The document is split so
that the two kinds of content never get confused again.

- **GIVEN** — facts and story from the author. Not to be altered, extended, or
  embellished by anyone else.
- **PROPOSED** — mechanics and technical design, offered and revisable.
- **OPEN** — questions waiting on the author.

Nothing moves from OPEN or PROPOSED into GIVEN except by the author saying it.

---

# PART ONE — GIVEN

## 1. The premise

Gumball wakes up in a strange place. It is a grassy field with a patch of
fresh-turned soil in it, and it has all grown wild. His four family members were
out there with him.

He has to find each of them.

He remembers being naughty. He remembers hurting and annoying them, and he is
worried they will not think he is a good boy.

He rescues them one by one. Each one tells him he is a good boy.

The reveal at the end is that it is sort of like dog heaven, and that he has been
trying to find his family and reunite with them.

## 2. The family

Four members. They are referred to by role, not by name.

- **the Boy**
- **the Girl**
- **the Mother**
- **the Father**

## 3. What Gumball did

| To whom | What he did |
|---|---|
| the Boy | Bit his hand. |
| the Girl | Got into her trash can, all the time. |
| the Mother | Ate her chocolates, which miraculously did not kill him. |
| the Father | Bit him, and ate all his Easter cakes. |

## 4. The black cat

An enigmatic black cat sows doubts in Gumball's mind along the way.

## 5. Carrots

Carrots are Gumball's power-ups and his health. He loved them.

## 6. Structure

- Four family members, one set of levels each, each with their own personal
  theme.
- Three levels per world.
- A boss at the end of the third level, standing between Gumball and the family
  member.
- After the boss, he finds them. He shies away for a moment. They pet his head
  and hug him and tell him he is a good boy.

## 7. What is actually true about Gumball

- He died in his sleep, at home.
- None of the family had a chance to say goodbye.
- He was buried in the rain and the mud, and it was very sad.

---

# PART TWO — PROPOSED

Everything below this line is design work, not story. Cut freely.

## 8. What the sins give us mechanically

Reading only the four things in section 3, two verbs come out and nothing else
does.

**BITE.** The Boy's hand. The Father.
**EAT.** The trash can. The chocolates. The Easter cakes.

Two people got bitten. Everybody got eaten from. That is the actual shape of his
guilt and it is a better shape than four unrelated mechanics, because it means
the two verbs recur and deepen instead of being introduced and discarded.

The Father is the only one who got both, which puts his world last by
construction rather than by decision.

### The tension already in the material

Carrots are health. Eating is the sin. Gumball heals by doing the thing he is
ashamed of, and the game does not have to say a word about that for a player to
feel it.

How hard to lean on this is an open question. The light version is that it simply
sits there. The heavy version makes eating a resource the player has to spend
guiltily. **OPEN.**

### The chocolates

A food that hurts him and that he ate anyway and survived. Mechanically that is a
pickup that costs health instead of restoring it, and it is the only one in the
game. Whether to use it at all is the author's call, because it is close to the
bone. **OPEN.**

## 9. The three-level shape

A template so each world reads the same way and anticipation does the work.

- **Level 1 — Teach.** The world's use of the verb, in a safe space. No cat, no
  pressure.
- **Level 2 — Complicate.** The verb gets a cost. The black cat appears once.
- **Level 3 — The boss.** Ends with the boss, then the reunion.

## 10. The reunion

The beats are GIVEN (shy away, pet, hug, good boy). This is the staging.

**Beat 1.** The boss stops. All music cuts. Not a fade, a cut. Silence.

**Beat 2.** The family member is standing there doing something ordinary.

**Beat 3.** The player has control. No prompt, no arrow, no button hint. You walk
to them yourself. Most players hesitate, and the hesitation is the design.

> If the player stands still, the family member waits. Forever. They never leave
> and they never call out.

**Beat 4.** Within about three tiles, control is taken for the **shy-away**: he
stops, ears flatten, head turns aside, tail tucks. He is bracing to be scolded.

**Beat 5.** The hand comes down. He flinches into it. Pet, hug, and the line.

Then: max carrots +1 and the bar visibly grows, the idle sprite upgrades, and the
world theme never plays again.

**The dialogue is the author's to write.** Nothing in this document should
contain a line spoken by a member of the family.

## 11. Two animation arcs

The cheapest high-value ideas here, and they reinforce each other.

**The idle sprite.** Starts with tail down, ears flat, head low. After each
reunion the idle changes: ears up, head lifts, tail begins to move, and after the
fourth it wags.

**The shy-away shrinks.** World 1 it is a full second and he almost backs up.
World 4 it is one frame of hesitation before he is already moving.

A handful of tiles each, no dialogue, and every player will notice.

## 12. Carrots as a system

- Health bar is a row of carrot icons, not hearts.
- Small carrot restores one.
- **Every reunion permanently raises max carrots by one.** Four family members,
  four extra carrots. Nearly free to implement, and it means being told he is a
  good boy makes him measurably stronger.

## 13. The bosses

Four are needed, one per world, standing between Gumball and the family member.

The four I proposed earlier were built on themes I invented and those themes are
now void. The slots stand empty pending the author's world themes.

One principle worth keeping, if the author wants it: **the sins are what Gumball
did, and the bosses could be what Gumball was afraid of.** Real dog fears, made
out of each world's material, rather than invented monsters. That keeps the whole
game inside a dog's actual experience.

**OPEN: the four world themes, from which the bosses follow.**

### Boss constraints regardless of what they are
- Three phases, one hit per phase. The reward is the reunion, not the challenge.
- Beaten using that world's verb, so the mechanic he is ashamed of is the
  mechanic that saves them.

## 14. What the game should probably never show

A restraint guard, given section 7.

- The death, in any form.
- The body.
- A funeral, a graveside, or a grave marker.
- The words "died," "dead," or "heaven."
- Gumball working it out and saying it aloud.

The game runs on the player knowing something the main character does not. One
plain sentence collapses that.

## 15. Game Boy technical notes

Target: DMG (original Game Boy), MBC5, 512 KB. Runs on GBC and Super Game Boy
without needing to.

### Sprites
- 160x144, 4 shades, 8x8 background tiles.
- Hardware limit: **40 sprites total, 10 per scanline.** The per-scanline limit is
  the one that will actually bite you.
- Use **8x16 sprite mode** (LCDC bit 2). A 16x16 Gumball is 2 hardware sprites
  instead of 4.
- Budget: Gumball 2-4 sprites, at most 3 enemies at 2-4 each, and hold a reserve
  for pickups. Design so vertical stacking is rare.

### Bosses on DMG
The part that will humble you if you plan it late.

- **Draw the boss body in the background layer, not with sprites.** A big
  sprite-built object blows the 10-per-scanline limit instantly. Animate by
  rewriting BG tiles during VBlank and by scrolling.
- Sprites only for small moving parts.
- One boss tileset per world, swapped in when level 3 loads. Fits comfortably in
  512 KB.
- Three phases, one hit each. No boss health bar.

### Build the text engine first
The emotional payload of this game is text. A variable-width font, a typewriter
reveal, a portrait window, and a simple script format are the *first* thing to
build, not the last. If the text engine is bad the game is bad no matter how the
platforming feels.

### Audio
Four channels. Write **one** Gumball melody and make each world theme a variation
of it. Each reunion gets a short motif from that melody. The finale is the full
arrangement, the first time it is heard whole.

Silence is a real instrument here. The music cut at the reunion is written into
the audio design, not an accident of it.

### Assembly vs C
- **To learn the hardware:** RGBDS assembly. Twelve levels and four bosses is a
  lot for a first Game Boy project, so be willing to cut a world to two levels if
  month four arrives and you are not halfway.
- **To finish the memorial:** GBDK-2020 (C) for game logic, hand-written ASM for
  the renderer, the scroll, and collision. Several times faster and the tight
  parts stay tight.

Prototype **world 1 level 1 and the bite** first. Prototype **the reunion**
second, with programmer art, because it is what the game is for.

## 16. Scope

| Piece | Count |
|---|---|
| Opening field | 1 map |
| Worlds | 4 |
| Levels per world | 3 |
| World levels | 12 maps |
| Finale | 1 map |
| **Total playable maps** | **14** |
| Bosses | 4 |
| Reunions | 4 |
| Black cat scenes | 8 |

Ship a vertical slice of world 1 first: three levels, the boss, and the reunion.
Everything after is a proven shape in different art.

## 17. Risks

1. **The first 80% has to be fun without the reveal.** Guilt does not entertain
   for three hours on its own. The verbs have to carry it.
2. **Four bosses is where a first Game Boy project dies.** Build one as a
   background-layer object early and prove the technique before designing the
   rest around it.
3. **The cat can become preachy.** Cap it at two appearances per world, each
   shorter than the last.
4. **Sentiment can outrun the game.** The idle sprite, the shrinking shy-away and
   the growing carrot bar say it without anyone stating it.
5. **Never let the player fail a reunion.** No family member withholds it and
   nothing can kill you on the walk toward them.

---

# PART THREE — OPEN

Waiting on the author.

1. **The four world themes.** Each family member's own personal theme. The bosses
   and the level art follow from these and nothing else should be guessed.
2. **World order.** BITE and EAT suggest the Father last, since he is the only
   one who got both. Confirm or overrule.
3. **The opening.** He wakes in the field with the fresh-turned soil. How much of
   that is playable, and what he does first.
4. **The cat.** What it says, and what happens to it.
5. **The ending.** After the fourth reunion.
6. **The eating tension.** Whether carrots-as-health against eating-as-sin gets
   leaned on or left alone.
7. **The chocolates.** Whether they appear in the game at all.
8. **All dialogue.**

---

*For Gumball. Good boy.*
