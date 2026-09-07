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

## 4. Who they were to him

One section per family member, filled in from the author only.

### the Boy

- Gumball would protect the Boy and the Girl when he thought roughhousing with
  the Father had got too serious.
- He wanted to join in. He would bark and run around with them.
- He played with squeaking stuffed animals with the Boy and the Girl.
- He played chase around the downstairs when they had a toy or a snack he
  wanted. Usually carrots or bread.
- He eagerly waited for the Boy and the Girl to come home from school.

### the Girl

- Gumball would protect the Boy and the Girl when he thought roughhousing with
  the Father had got too serious.
- He wanted to join in. He would bark and run around with them.
- He slept in the Girl's room some nights, or downstairs on the couch if she
  slept there.
- He played with squeaking stuffed animals with the Boy and the Girl.
- He played chase around the downstairs when they had a toy or a snack he
  wanted. Usually carrots or bread.
- He eagerly waited for the Boy and the Girl to come home from school.

### the Mother

- He would hang with the Mother while she watched shows on the couch.
- He kept her company when the Father was away on trips.
- She would help trim his fur when it needed it.

### the Father

- Gumball was his best friend. His shadow.
- He would sit outside the Father's office, keeping an eye out.
- They went on hikes in the desert.
- They went on rides in the car.
- Gumball ate vegetable scraps when the Father cooked and prepared meals.

## 4a. Who Gumball was

The author's account of the dog himself.

- He was a grumpy dog.
- He was not always kind to everyone.
- He growled, and he would snap.
- **He was not the best dog. But he did love them all.**
- When they came home he would hop up and sniff their faces intently.
- He would almost never lick.
- He would paw at the back door to go out. This was often a trick, because he
  would then lead you to the fridge for a carrot.
- **He was loyal, and he missed his family when they were away.**

### The thesis

> He was not the best dog. But he did love them all.

That line is the game. It is the author's, it is quoted here verbatim, and the
design answers to it.

It means the reunions must not say "you were never bad." He was difficult, and
the record is not in dispute. What the family gives him is not a correction. It
is love that already knew.

Any writing that softens him is wrong.

## 5. The black cat

An enigmatic black cat sows doubts in Gumball's mind along the way.

## 6. Carrots

Carrots are Gumball's power-ups and his health. He loved them.

## 7. Structure

- Four family members, one set of levels each, each with their own personal
  theme.
- Three levels per world.
- A boss at the end of the third level, standing between Gumball and the family
  member.
- After the boss, he finds them. He shies away for a moment. They pet his head
  and hug him and tell him he is a good boy.

## 8. What is actually true about Gumball

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

## 8a. What the Father's material gives us

Reading only section 4, without adding to it.

### Three settings for three levels
The office, the desert, and the car are three places, and the world needs three
levels. That mapping comes from the author's list rather than from a designer's
guess. Whether the car is a level or the thing that connects them is **OPEN**.

The desert is also the only outdoor, non-domestic setting given so far. On a
4-shade screen it is the one world that can be mostly empty, which is worth
having when the other three sound like interiors.

### "His shadow" is a mechanic, not a description
Gumball's whole behaviour with the Father was following. The entire game is
Gumball looking for people who are not there. If the author wants it, the
Father's world is the one that names that: he was a shadow, and a shadow needs
somebody in front of it.

Nothing in this document decides how that gets used. **OPEN.**

### Keeping an eye out
He sat outside the office on his own initiative. He had a job he gave himself.
That is a posture, an idle animation, and possibly a verb, and it is the only
thing in the material where Gumball is being good on purpose rather than being
bad by accident.

### The carrots may already belong to the Father
Two GIVEN facts touch:

- Carrots are health, because he loved them (section 6).
- He ate vegetable scraps when the Father cooked (section 4).

If the author wants the connection, the health system originates in the Father's
kitchen, and the player has been picking up his food since the first screen
without knowing it. It costs nothing to implement, because the mechanic already
exists; it is purely a matter of what the last world reveals.

**OPEN, and the author's call entirely.** It is noted here only because it is a
consequence of two things the author already said, not an addition to them.

### The closest one got the worst of it
Section 3 says Gumball bit the Father and ate his Easter cakes. Section 4 says
the Father was his best friend. Both are the author's. The design does not need
to comment on it, and should not.

## 8b. What Gumball's character gives us

Reading only section 4a, without adding to it.

### GROWL is a verb, and it is not BITE
He growled *and* he snapped. Those are two different things a dog does, and the
second is what he is ashamed of. A growl is a warning that stops short. That is a
real mechanic: make a thing back off without hurting it.

If the game has both, then every encounter has a version where Gumball threatens
and a version where he bites, and the player chooses. The game never has to score
that choice or comment on it. **OPEN.**

### The back door trick is a puzzle
He pawed the back door to go out. He did not want to go out. He wanted you to
follow him to the fridge.

That is a complete puzzle mechanic already: **ask for the wrong thing on purpose
so someone follows you to the right thing.** It is misdirection, it is
non-violent, it is funny, and it is the one piece of the material where he is
cleverer than the humans. It also pairs with "his shadow" from section 4 by
inverting it, since here he is the one being followed.

Whether it appears, and in whose world, is **OPEN**.

### Carrots are load-bearing
Carrots have now come up three separate times in the author's own account: as
health, as the vegetable scraps at the Father's feet, and as the thing at the end
of the back-door trick. They are not a themed pickup. They are the currency of
the whole relationship, and the health system is already the right home for them.

### The sins are a temperament, not four incidents
Section 3 lists two bites. Section 4a says he growled and snapped and was not
always kind. So the bites are not aberrations, they are who he was on a bad day.

This is good for the design, because BITE can then be the core verb of the whole
game rather than one world's gimmick, and the player is holding his temperament
in their hands for twelve levels.

### It also gives the black cat real material
The cat sows doubt (section 5) and, per the mechanics below, works best saying
true things. Section 4a is a supply of true things that are genuinely unkind.
The design notes this and writes none of them. **The cat's dialogue is the
author's.**

## 8c. What the family material gives us

Reading only section 4, without adding to it.

### The through-line is WAITING

**This is now GIVEN, not inferred.** The author states it directly in section 4a:
he was loyal, and he missed his family when they were away.

It was already present four separate times in the relationship material before
being said outright:

| Who | What he did |
|---|---|
| the Father | Sat outside his office, keeping an eye out. |
| the Boy and the Girl | Eagerly waited for them to come home from school. |
| the Mother | Kept her company when the Father was away on trips. |

Four people, one behaviour. This dog held a position until somebody came back.
It is the only thing every relationship in the material has in common.

It is also the only virtue in the entire account of him. Section 4a is otherwise
grumpy, snapped, not always kind, not the best dog. Loyal is the counterweight,
and it is what makes the premise function: a loyal dog wakes up alone and the
people are gone.

The game is a dog alone in a field waiting to be found. The design does not need
to say a word about that, and probably should not, but every level should know
it.

It also means the reunion staging in section 10 is already the right shape: the
family member stands still and waits forever. That beat rhymes with his whole
life instead of just being a nice idea.

Still **OPEN**: whether the game ever acknowledges this out loud, or whether it
stays entirely in the level design and the animation. The recommendation is that
nobody ever says it.

### CHASE is the Boy and Girl's verb, and it runs backwards

He played chase around the downstairs when they had something he wanted. In life
he chased them. The entire game is him looking for them and not finding them.

That inversion is free. It costs nothing to build and nobody has to point at it.

### The Mother's world is the still one

Her material has no running, no chasing, no roughhousing, no barking. She is
sitting on a couch watching shows, and he is next to her. Then she trims his fur,
which means holding still and being handled and tolerating it.

Three of the four worlds are loud. Hers is not, and the design should protect
that rather than inventing action for her. A quiet world in the third slot is
also good pacing.

### The squeak

Squeaking stuffed animals, shared between the Boy and the Girl. On a four-channel
chip a squeak is nearly free and instantly recognisable. It is a sound cue, a
pickup, and a thing to carry.

### Bread joins the carrots

"Usually carrots or bread." Carrots have now appeared four times in the author's
account. Bread is the first alternative and the only other named food he wanted.

### The Boy's material is thin

The Boy currently shares everything he has with the Girl, plus the bitten hand.
The Girl has a room and a couch and a trash can of her own; the Father has an
office, a desert, a car and a kitchen; the Mother has her couch and the fur
trimming.

**OPEN, and the most useful thing the author could add.** Right now the Boy's
world would have to be built out of shared material, which will make worlds one
and two feel like the same house twice.

### One adjacency, noted and not used

Section 3: he bit the Father.
Section 4: he protected the Boy and the Girl when roughhousing with the Father
got too serious.

Both are the author's. Put together they supply a reason, and reasons are story.
**The design will not use this unless the author says to.** It is recorded here
only so it is not accidentally contradicted later.

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

**Beat 6. The greeting.** He hops up and sniffs their face, intently. This is
what he actually did when they came home (section 4a) and it is the correct end
of a reunion in a game about coming home. He does not lick. He almost never
licked, so the game never shows it.

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
- Gumball licking anyone. He almost never licked. The sniff is the greeting.
- Gumball being sweetened. He growled and he snapped and that stays true.

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

1. **More material for the Boy**, per section 8c. He and the Girl currently
   share almost everything, which risks worlds one and two feeling like the same
   house twice.
1a. **Is the car a level, or the thing that connects the Father's three?**
1b. **Whether WAITING is ever acknowledged out loud.** It is now GIVEN as
   character (section 4a). The open part is only whether the game names it or
   leaves it in the level design. Recommendation: never name it.
1c. **Was "park and run around with them" meant as bark?** Recorded as bark.
2. **World order.** BITE and EAT suggest the Father last, since he is the only
   one who got both. Confirm or overrule.
3. **The opening.** He wakes in the field with the fresh-turned soil. How much of
   that is playable, and what he does first.
4. **The cat.** What it says, and what happens to it.
5. **The ending.** After the fourth reunion.
6. **The eating tension.** Whether carrots-as-health against eating-as-sin gets
   leaned on or left alone.
6a. **Whether the carrots come from the Father**, per section 8a.
6b. **Whether "his shadow" and "keeping an eye out" become mechanics** or stay
   as character.
6c. **Whether GROWL exists alongside BITE**, per section 8b.
6d. **Whether the back-door trick becomes a puzzle mechanic**, and in whose
   world.
7. **The chocolates.** Whether they appear in the game at all.
8. **All dialogue.**

---

*For Gumball. Good boy.*
