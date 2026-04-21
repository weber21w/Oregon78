#ifndef OREGON78_H
#define OREGON78_H

#include <uzebox.h>
#include <keyboard.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>

typedef s32 fx32;
typedef long long s64;

#define OREGON_FX_SHIFT	8
#define OREGON_FX_ONE	(1L << OREGON_FX_SHIFT)

extern u16 GetVsyncCounter(void);
extern void ClearVsyncCounter(void);

#ifndef OREGON_SCREEN_W
#define OREGON_SCREEN_W	SCREEN_TILES_H
#endif

#ifndef OREGON_SCREEN_H
#define OREGON_SCREEN_H	SCREEN_TILES_V
#endif

#define OREGON_GET_KEY()	OregonGetKeyBlocking()

/* Optional tuning for the timed shot. */
#ifndef OREGON_KEYBOARD_SHOT_FRAMES_PER_UNIT
#define OREGON_KEYBOARD_SHOT_FRAMES_PER_UNIT	24u
#endif

#ifndef OREGON_JOYPAD_SHOT_FRAMES_PER_UNIT
#define OREGON_JOYPAD_SHOT_FRAMES_PER_UNIT	6u
#endif

#ifndef OREGON_SOURCE_BUG_COMPAT
#define OREGON_SOURCE_BUG_COMPAT	0u
#endif

#ifndef OREGON_SOURCE_INPUT_QUIRKS
#define OREGON_SOURCE_INPUT_QUIRKS	0u
#endif

#ifndef OREGON_USE_TRUE_RANDOM_SEED
#define OREGON_USE_TRUE_RANDOM_SEED	1u
#endif

#ifndef OREGON_FALLBACK_SEED
#define OREGON_FALLBACK_SEED	0xACE1u
#endif



enum{
	OREGON_INPUT_KEYBOARD = 0,
	OREGON_INPUT_JOYPAD = 1
};


/* ------------------------------------------------------------------------- */
/* Game state                                                                */
/* ------------------------------------------------------------------------- */

enum{
	DEATH_NONE = 0,
	DEATH_STARVATION,
	DEATH_INJURY,
	DEATH_PNEUMONIA,
	DEATH_SNAKEBITE,
	DEATH_WINTER,
	DEATH_MASSACRE
};

typedef struct OregonGame{
	fx32 oxen_spend;			/* original: A */
	fx32 bullets;				/* original: B */
	fx32 clothing;				/* original: C */
	fx32 food;					/* original: F */
	fx32 mileage;				/* original: M */
	fx32 misc_supplies;		/* original: M1 */
	fx32 prev_mileage;		/* original: M2 */
	fx32 cash;					/* original: T */

	s16 shot_time;				/* original: B1 */

	u8 insufficient_clothing_flag;	/* original: C1 */
	u8 event_roll_index;			/* original: D1 */
	u8 turn_index;				/* original: D3 */
	u8 shooting_skill;			/* original: D9 */
	u8 eating_choice;			/* original: E */
	u8 south_pass_cleared;		/* original: F1 */
	u8 blue_mountains_cleared;	/* original: F2 */
	u8 final_turn_fraction;		/* original: F9 */
	u8 injury_flag;				/* original: K8 */
	u8 blizzard_flag;			/* original: L1 */
	u8 display_950_flag;		/* original: M9 */
	u8 illness_flag;			/* original: S4 */
	u8 riders_non_hostile_flag;	/* original: S5 */
	u8 shot_word_index;			/* original: S6 */
	u8 rider_tactic;			/* original: T1 */
	s8 fort_option_toggle;		/* original: X1 */

	u8 game_over;
	u8 won;
	u8 death_reason;
} OregonGame;

/*
 * BASIC-to-C state name map:
 * A  -> oxen_spend
 * B  -> bullets
 * C  -> clothing
 * F  -> food
 * M  -> mileage
 * M1 -> misc_supplies
 * M2 -> prev_mileage
 * T  -> cash
 * B1 -> shot_time
 * B3 -> not stored explicitly; ClearVsyncCounter()/GetVsyncCounter() timing
 * D3 -> turn_index
 * D9 -> shooting_skill
 * E  -> eating_choice
 * F1 -> south_pass_cleared
 * F2 -> blue_mountains_cleared
 * F9 -> final_turn_fraction
 * K8 -> injury_flag
 * L1 -> blizzard_flag
 * M9 -> display_950_flag
 * S4 -> illness_flag
 * S5 -> riders_non_hostile_flag
 * S6 -> shot_word_index
 * T1 -> rider_tactic
 * X1 -> fort_option_toggle
 */

static const char str_bang[] PROGMEM = "BANG";
static const char str_blam[] PROGMEM = "BLAM";
static const char str_pow[] PROGMEM = "POW";
static const char str_wham[] PROGMEM = "WHAM";

static const char *const shot_words[] PROGMEM ={
	str_bang,
	str_blam,
	str_pow,
	str_wham
};

static const u8 event_thresholds[] PROGMEM ={
	6, 11, 13, 15, 17, 22, 32, 35, 37, 42, 44, 54, 64, 69, 95
};

/* ------------------------------------------------------------------------- */
/* Function declarations                                                     */
/* ------------------------------------------------------------------------- */

#define ConPrintL(lit)		ConPrint_P(PSTR(lit))
#define PrintCenteredL(lit,pad)	PrintCentered_P(PSTR(lit),(pad))
#define StreqL(left,lit)		Streq_P((left),PSTR(lit))
#define ReadFortSpendL(lit)	ReadFortSpend_P(PSTR(lit))

static void OregonSyncPrngVsyncMark(void);

static void OregonAdvancePrngPerElapsedFrame(void);

static u8 OregonGetKeyNonBlocking(void);

static void OregonSyncJoypadState(void);

static u8 OregonGetShotFramesPerUnit(void);

static u16 OregonReadJoypadPressed(void);

static u16 OregonGetJoypadPressedBlocking(void);

static u8 OregonGetKeyBlocking(void);

static void ConClearLine(u8 row_index);

static void ConClear(void);

static void ConEnsureLinesAvailable(u8 needed_line_count);

static void ConPageBreak(void);

static void ConAdvanceLine(void);

static void ConPut(char c);

static void ConPrint(const char *s);

static void ConPrint_P(PGM_P s);

static void ConNewLine(void);

static void ConSpaces(u8 space_count);

static void ConPrintNum(s16 v);

static void PrintCentered_P(PGM_P s, u8 left_padding);

static fx32 FxFromInt(s16 value_int);

static fx32 FxFloorValue(fx32 value_fx);

static s16 FxToIntFloor(fx32 value_fx);

static fx32 RandFx(s16 range_int);

static void StrToUpper(char *s);

static u8 Streq(const char *left, const char *right);

static u8 Streq_P(const char *left, PGM_P right);

static u16 Rand16(void);

static u8 RandPct(void);

static u8 RandPctLess(u8 pct);

static void MarkDead(u8 reason);

static void JoypadRenderIntField(u8 x, u8 y, const char *digits, u8 cursor_pos);

static s16 JoypadDigitsToInt(const char *digits);

static void JoypadFormatInt(char *dst, s16 value);

static s16 ReadIntJoypad(void);

static void WaitForContinueJoypad(void);

static void SelectInputMode(void);

static void ReadLine(char *dst, u8 max_len, u8 force_uppercase);

static s16 ReadIntRaw(void);

static void WaitForEnter(void);

static void WaitForPageContinue(void);

static void PrintDateTurn(u8 turn_index);

static void PrintInventory(void);

static void InitGame(void);

static void PrintInstructions(void);

static void AskInstructions(void);

static void AskShotSkill(void);

static void DoInitialPurchases(void);

static void ShootSubroutine(void);

static void DoNoDoctorDeath(void);

static void DoNoMedicineDeath(void);

static void DoIllness(void);

static fx32 ReadFortSpend_P(PGM_P label);

static void DoFort(void);

static void DoHunt(void);

static void RidersPhase(void);

static void DoEvent(void);

static void DoMountains(void);

static void TurnStart(void);

static u8 AskAction(void);

static void AskEat(void);

static void AdvanceDate(void);

static void DoWinSequence(void);

static void DoDeathFormality(void);

static void PlayOneTurn(void);

int main(void);

#endif /* OREGON78_H */
