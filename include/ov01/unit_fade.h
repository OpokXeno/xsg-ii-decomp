#ifndef INCLUDE_OV01_UNIT_FADE_H
#define INCLUDE_OV01_UNIT_FADE_H

/* Functions of src/ov01/unit_fade.c that other files call. */

struct BattleUnit;
struct FadeTask;

struct FadeTask *unitFadeIn(struct BattleUnit *unit);
struct FadeTask *unitFadeTo(struct BattleUnit *unit, int frames, int targetPercent);
struct FadeTask *unitFadeOut(struct BattleUnit *unit);
struct FadeTask *unitSlotFadeIn(struct BattleUnit *unit, int index);
struct FadeTask *unitSlotFadeOut(struct BattleUnit *unit, int index);

#endif /* INCLUDE_OV01_UNIT_FADE_H */
