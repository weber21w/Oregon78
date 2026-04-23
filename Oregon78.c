/*
 * Oregon Trail (1978 BASIC) - Uzebox Mode 90 text port
 *
 * Based on: https://github.com/clintmoyer/oregon-trail
 */

#include "Oregon78.h"

static u16 OregonPrngVsyncMark;
static u8 OregonInputMode;
static u8 OregonScreenOffset;
static u16 OregonPrevJoypadState;

static OregonGame g;
static u8 cursor_x;
static u8 cursor_y;

static void OregonSyncPrngVsyncMark(void){
	OregonPrngVsyncMark = GetVsyncCounter();
}


static void OregonAdvancePrngPerElapsedFrame(void){
	u16 current_vsync_count = GetVsyncCounter();

	if(current_vsync_count < OregonPrngVsyncMark){
		OregonPrngVsyncMark = current_vsync_count;
	}

	while(OregonPrngVsyncMark != current_vsync_count){
		GetPrngNumber(0);
		KeyboardPoll();
		++OregonPrngVsyncMark;
	}
}



static u8 OregonGetKeyNonBlocking(void){
	if(KeyboardHasKey()){
		return KeyboardGetKey(true);
	}

	return 0;
}


static void OregonSyncJoypadState(void){
	OregonPrevJoypadState = ReadJoypad(0);
}

static u8 OregonGetTextWidth(void){
	u8 text_width = (u8)(OREGON_SCREEN_W - OregonScreenOffset);

	if(text_width == 0){
		return 1;
	}

	return text_width;
}


static u8 OregonGetScreenX(u8 logical_x){
	return (u8)(logical_x + OregonScreenOffset);
}


static u8 OregonGetShotFramesPerUnit(void){
	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		return (u8)OREGON_JOYPAD_SHOT_FRAMES_PER_UNIT;
	}

	return (u8)OREGON_KEYBOARD_SHOT_FRAMES_PER_UNIT;
}


static u16 OregonReadJoypadPressed(void){
	u16 current_joypad_state = ReadJoypad(0);
	u16 newly_pressed = (u16)(current_joypad_state & (u16)~OregonPrevJoypadState);
	OregonPrevJoypadState = current_joypad_state;
	return newly_pressed;
}


static u16 OregonGetJoypadPressedBlocking(void){
	while(1){
		u16 newly_pressed;

		OregonAdvancePrngPerElapsedFrame();
		newly_pressed = OregonReadJoypadPressed();
		if(newly_pressed != 0)
		{
			return newly_pressed;
		}

		WaitVsync(1);
	}
}


static u8 OregonGetKeyBlocking(void){
	while(1){
		u8 translated_key;

		OregonAdvancePrngPerElapsedFrame();
		translated_key = OregonGetKeyNonBlocking();
		if(translated_key != 0)
		{
			return translated_key;
		}

		WaitVsync(1);
	}
}


static void ConClearLine(u8 row_index){
	u8 column_index;

	for(column_index = 0; column_index < OREGON_SCREEN_W; ++column_index){
		PrintChar(column_index, row_index, ' ');
	}
}


static void ConClear(void){
	u8 row_index;

	for(row_index = 0; row_index < OREGON_SCREEN_H; ++row_index){
		ConClearLine(row_index);
	}

	cursor_x = 0;
	cursor_y = 0;
}


static void ConEnsureLinesAvailable(u8 needed_line_count){
	if(needed_line_count == 0){
		return;
	}

	if(cursor_y > (u8)((OREGON_SCREEN_H - 2) - (needed_line_count - 1))){
		ConPageBreak();
	}
}


static void ConPageBreak(void){
	ConClearLine((u8)(OREGON_SCREEN_H - 1));
	cursor_x = 0;
	cursor_y = (u8)(OREGON_SCREEN_H - 1);
	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		ConPrintL("-= PRESS A OR START TO CONTINUE =-");
	}else{
		ConPrintL("-= PRESS ENTER TO CONTINUE =-");
	}
	WaitForPageContinue();
	ConClear();
}


static void ConAdvanceLine(void){
	cursor_x = 0;
	if(cursor_y < (u8)(OREGON_SCREEN_H - 2)){
		++cursor_y;
	}else{
		ConPageBreak();
	}
}


static void ConPut(char c){
	if(c == '\r')
		return;

	if(c == '\n'){
		ConAdvanceLine();
		return;
	}

	if(c == 8){
		if(cursor_x){
			--cursor_x;
		}else if(cursor_y){
			--cursor_y;
			cursor_x = (u8)(OregonGetTextWidth() - 1);
		}
		PrintChar(OregonGetScreenX(cursor_x), cursor_y, ' ');
		return;
	}

	PrintChar(OregonGetScreenX(cursor_x), cursor_y, (u8)c);
	++cursor_x;
	if(cursor_x >= OregonGetTextWidth()){
		ConAdvanceLine();
	}
}


static void ConPrint(const char *s){
	while(*s){
		ConPut(*s++);
	}
}

static void ConPrint_P(PGM_P s){
	char c;

	while((c = (char)pgm_read_byte(s++)) != 0){
		ConPut(c);
	}
}


static void ConNewLine(void){
	ConPut('\n');
}


static void ConSpaces(u8 space_count){
	while(space_count--){
		ConPut(' ');
	}
}


static void ConPrintNum(s16 v){
	char digits[8];
	u8 digit_count = 0;
	u16 value_abs;

	if(v == 0){
		ConPut('0');
		return;
	}

	if(v < 0){
		ConPut('-');
		value_abs = (u16)(-v);
	}else{
		value_abs = (u16)v;
	}

	while(value_abs && digit_count < (u8)(sizeof(digits) - 1)){
		digits[digit_count++] = (char)('0' + (value_abs % 10));
		value_abs /= 10;
	}

	while(digit_count){
		ConPut(digits[--digit_count]);
	}
}


static void PrintCentered_P(PGM_P s, u8 left_padding){
	ConSpaces(left_padding);
	ConPrint_P(s);
	ConNewLine();
}


static fx32 FxFromInt(s16 value_int){
	return ((fx32)value_int) << OREGON_FX_SHIFT;
}


static fx32 FxFloorValue(fx32 value_fx){
	if(value_fx >= 0){
		return value_fx & ~((fx32)OREGON_FX_ONE - 1);
	}

	return (fx32)(-((((-value_fx) + ((fx32)OREGON_FX_ONE - 1)) >> OREGON_FX_SHIFT) << OREGON_FX_SHIFT));
}


static s16 FxToIntFloor(fx32 value_fx){
	if(value_fx >= 0){
		return (s16)(value_fx >> OREGON_FX_SHIFT);
	}

	return (s16)(-(((-value_fx) + ((fx32)OREGON_FX_ONE - 1)) >> OREGON_FX_SHIFT));
}


static fx32 RandFx(s16 range_int){
	return (fx32)(((u32)Rand16() * (u32)range_int) >> 8);
}


static void StrToUpper(char *s){
	while(*s){
		if(*s >= 'a' && *s <= 'z'){
			*s = (char)(*s - ('a' - 'A'));
		}
		++s;
	}
}


static u8 Streq(const char *left, const char *right){
	while(*left && *right){
		if(*left != *right) return 0;
		++left;
		++right;
	}

	return (*left == 0 && *right == 0) ? 1 : 0;
}

static u8 Streq_P(const char *left, PGM_P right){
	char right_char;

	while(*left){
		right_char = (char)pgm_read_byte(right);
		if(right_char == 0) return 0;
		if(*left != right_char) return 0;
		++left;
		++right;
	}

	return (pgm_read_byte(right) == 0) ? 1 : 0;
}


static u16 Rand16(void){
	return GetPrngNumber(0);
}


static u8 RandPct(void){
	return (u8)(Rand16() % 100u);
}


static u8 RandPctLess(u8 pct){
	return (u8)(RandPct() < pct);
}


static void MarkDead(u8 reason){
	g.game_over = 1;
	g.death_reason = reason;
}


static void JoypadRenderIntField(u8 x, u8 y, const char *digits, u8 cursor_pos){
	char field_buf[7];
	u8 blink_on = (u8)((GetVsyncCounter() >> 4) & 1u);
	u8 digit_index;

	field_buf[0] = '[';
	for(digit_index = 0; digit_index < 4; ++digit_index){
		char c = digits[digit_index];
		if(digit_index == cursor_pos && blink_on){
			c = '_';
		}
		field_buf[1 + digit_index] = c;
	}
	field_buf[5] = ']';
	field_buf[6] = 0;
	PrintRam(OregonGetScreenX(x), y, (unsigned char *)field_buf);
}


static s16 JoypadDigitsToInt(const char *digits){
	return (s16)((digits[0] - '0') * 1000 + (digits[1] - '0') * 100 + (digits[2] - '0') * 10 + (digits[3] - '0'));
}


static void JoypadFormatInt(char *dst, s16 value){
	char temp[6];
	u8 start_index = 0;
	u8 out_index = 0;

	if(value < 0)
		value = 0;
	temp[0] = (char)('0' + ((value / 1000) % 10));
	temp[1] = (char)('0' + ((value / 100) % 10));
	temp[2] = (char)('0' + ((value / 10) % 10));
	temp[3] = (char)('0' + (value % 10));
	temp[4] = 0;

	while(start_index < 3 && temp[start_index] == '0'){
		++start_index;
	}

	while(temp[start_index]){
		dst[out_index++] = temp[start_index++];
	}
	dst[out_index] = 0;
}


static s16 ReadIntJoypad(void){
	char digits[4] = { '0', '0', '0', '0' };
	char value_buf[6];
	u8 edit_x = cursor_x;
	u8 edit_y = cursor_y;
	u8 cursor_pos = 3;
	u8 last_cursor_pos = 0xff;
	u8 last_blink_on = 0xff;
	char last_digits[4] = { 0xff, 0xff, 0xff, 0xff };

	while(1){
		u16 newly_pressed;
		u8 blink_on;
		u8 redraw = 0;
		u8 digit_index;

		OregonAdvancePrngPerElapsedFrame();
		blink_on = (u8)((GetVsyncCounter() >> 4) & 1u);
		if((cursor_pos != last_cursor_pos) || (blink_on != last_blink_on)){
			redraw = 1;
		}
		for(digit_index = 0; digit_index < 4; ++digit_index){
			if(digits[digit_index] != last_digits[digit_index]){
				redraw = 1;
				break;
			}
		}
		if(redraw){
			JoypadRenderIntField(edit_x, edit_y, digits, cursor_pos);
			last_cursor_pos = cursor_pos;
			last_blink_on = blink_on;
			for(digit_index = 0; digit_index < 4; ++digit_index){
				last_digits[digit_index] = digits[digit_index];
			}
		}
		newly_pressed = OregonReadJoypadPressed();

		if(newly_pressed & BTN_LEFT){
			if(cursor_pos > 0)
				--cursor_pos;
		}
		if(newly_pressed & BTN_RIGHT){
			if(cursor_pos < 3)
				++cursor_pos;
		}
		if(newly_pressed & BTN_UP){
			if(digits[cursor_pos] >= '9')
				digits[cursor_pos] = '0';
			else
				++digits[cursor_pos];
		}
		if(newly_pressed & BTN_DOWN){
			if(digits[cursor_pos] <= '0') digits[cursor_pos] = '9';
			else --digits[cursor_pos];
		}
		if(newly_pressed & BTN_B){
			digits[cursor_pos] = '0';
		}
		if(newly_pressed & (BTN_A | BTN_START)){
			s16 value = JoypadDigitsToInt(digits);
			JoypadFormatInt(value_buf, value);
			for(digit_index = 0; digit_index < 6; ++digit_index){
				PrintChar(OregonGetScreenX((u8)(edit_x + digit_index)), edit_y, ' ');
			}
			PrintRam(OregonGetScreenX(edit_x), edit_y, (unsigned char *)value_buf);
			cursor_x = (u8)(edit_x + strlen(value_buf));
			cursor_y = edit_y;
			ConNewLine();
			return value;
		}

		WaitVsync(1);
	}
}


static void WaitForContinueJoypad(void){
	ConPrintL("-= PRESS A OR START TO CONTINUE =-");
	ConNewLine();
	while(1){
		u16 newly_pressed = OregonGetJoypadPressedBlocking();
		if(newly_pressed & (BTN_A | BTN_START)){
			return;
		}
	}
}


static void SelectScreenOffset(void){
	u8 selection = OregonScreenOffset;
	u8 last_drawn_selection = 0xff;

	OregonScreenOffset = 0;
	ConClear();
	ConPrintL("OREGON TRAIL");
	ConNewLine();
	ConNewLine();
	ConPrintL("SCREEN OFFSET?");
	ConNewLine();
	ConPrintL("0:NO  1:1  2:2  3:3");
	ConNewLine();
	ConNewLine();
	ConPrintL("KEYS: 0-3 OR ENTER   JOYPAD: LEFT/RIGHT + A/START");

	while(1){
		u8 key = OregonGetKeyNonBlocking();
		u16 newly_pressed;

		if(selection != last_drawn_selection){
			ConClearLine(6);
			cursor_x = 0;
			cursor_y = 6;
			ConPrintL("CURRENT: ");
			ConPrintNum(selection);
			last_drawn_selection = selection;
		}

		if(key >= '0' && key <= '3'){
			selection = (u8)(key - '0');
		}else if(key == '\r' || key == '\n'){
			OregonScreenOffset = selection;
			ConClear();
			return;
		}

		OregonAdvancePrngPerElapsedFrame();
		newly_pressed = OregonReadJoypadPressed();
		if(newly_pressed & (BTN_LEFT | BTN_UP)){
			if(selection > 0){
				--selection;
			}
		}
		if(newly_pressed & (BTN_RIGHT | BTN_DOWN)){
			if(selection < 3){
				++selection;
			}
		}
		if(newly_pressed & (BTN_A | BTN_START)){
			OregonScreenOffset = selection;
			ConClear();
			return;
		}

		WaitVsync(1);
	}
}


static void SelectInputMode(void){
	u8 selection = OREGON_INPUT_KEYBOARD;
	u8 last_drawn_selection = 0xff;

	ConClear();
	ConPrintL("OREGON TRAIL");
	ConNewLine();
	ConNewLine();
	cursor_x = 0;
	cursor_y = 6;
	ConClearLine(6);
	ConPrintL("KEYS: 1/2 OR ENTER   JOYPAD: D-PAD + A/START");

	while(1){
		u8 key = OregonGetKeyNonBlocking();
		u16 newly_pressed;

		if(selection != last_drawn_selection){
			ConClearLine(2);
			ConClearLine(3);
			ConClearLine(4);
			cursor_x = 0;
			cursor_y = 2;
			ConPrintL("SELECT CONTROL:");
			ConNewLine();
			if(selection == OREGON_INPUT_KEYBOARD){
				ConPrintL("> KEYBOARD");
				ConNewLine();
				ConPrintL("  JOYPAD");
			}else{
				ConPrintL("  KEYBOARD");
				ConNewLine();
				ConPrintL("> JOYPAD");
			}
			last_drawn_selection = selection;
		}

		if(key == '1')
			selection = OREGON_INPUT_KEYBOARD;
		else if(key == '2')
			selection = OREGON_INPUT_JOYPAD;
		else if(key == '\r' || key == '\n'){
			OregonInputMode = selection;
			ConClear();
			return;
		}

		OregonAdvancePrngPerElapsedFrame();
		newly_pressed = OregonReadJoypadPressed();
		if(newly_pressed & (BTN_LEFT | BTN_UP))
			selection = OREGON_INPUT_KEYBOARD;
		if(newly_pressed & (BTN_RIGHT | BTN_DOWN))
			selection = OREGON_INPUT_JOYPAD;
		if(newly_pressed & (BTN_A | BTN_START)){
			OregonInputMode = selection;
			ConClear();
			return;
		}

		WaitVsync(1);
	}
}


static void ReadLine(char *dst, u8 max_len, u8 force_uppercase){
	u8 line_length = 0;

	while(1){
		u8 input_char = OREGON_GET_KEY();

		if(input_char == '\r' || input_char == '\n'){
			dst[line_length] = 0;
			ConNewLine();
			return;
		}

		if(input_char == 8 || input_char == 127){
			if(line_length){
				--line_length;
				dst[line_length] = 0;
				ConPut(8);
				ConPut(' ');
				ConPut(8);
			}
			continue;
		}

		if(input_char < 32 || input_char > 126) continue;
		if(line_length >= (u8)(max_len - 1)) continue;

		if(force_uppercase && input_char >= 'a' && input_char <= 'z'){
			input_char = (u8)(input_char - ('a' - 'A'));
		}

		dst[line_length++] = (char)input_char;
		dst[line_length] = 0;
		ConPut((char)input_char);
	}
}


static s16 ReadIntRaw(void){
	char buf[16];

	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		return ReadIntJoypad();
	}

	ReadLine(buf, sizeof(buf), 0);
	return (s16)atoi(buf);
}


static void WaitForEnter(void){
	char input_buffer[2];

	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		WaitForContinueJoypad();
		return;
	}

	ReadLine(input_buffer, sizeof(input_buffer), 0);
}


static void WaitForPageContinue(void){
	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		while(1){
			u16 newly_pressed = OregonGetJoypadPressedBlocking();
			if(newly_pressed & (BTN_A | BTN_START)){
				return;
			}
		}
	}

	while(1){
		u8 key = OREGON_GET_KEY();
		if(key == '\r' || key == '\n' || key == 0x0d){
			return;
		}
	}
}


static void PrintDateTurn(u8 turn_index){
	ConPrintL("MONDAY ");

	switch(turn_index){
		case 0: ConPrintL("MARCH 29 1847"); break;
		case 1: ConPrintL("APRIL 12 1847"); break;
		case 2: ConPrintL("APRIL 26 1847"); break;
		case 3: ConPrintL("MAY 10 1847"); break;
		case 4: ConPrintL("MAY 24 1847"); break;
		case 5: ConPrintL("JUNE 7 1847"); break;
		case 6: ConPrintL("JUNE 21 1847"); break;
		case 7: ConPrintL("JULY 5 1847"); break;
		case 8: ConPrintL("JULY 19 1847"); break;
		case 9: ConPrintL("AUGUST 2 1847"); break;
		case 10: ConPrintL("AUGUST 16 1847"); break;
		case 11: ConPrintL("AUGUST 31 1847"); break;
		case 12: ConPrintL("SEPTEMBER 13 1847"); break;
		case 13: ConPrintL("SEPTEMBER 27 1847"); break;
		case 14: ConPrintL("OCTOBER 11 1847"); break;
		case 15: ConPrintL("OCTOBER 25 1847"); break;
		case 16: ConPrintL("NOVEMBER 8 1847"); break;
		case 17: ConPrintL("NOVEMBER 22 1847"); break;
		case 18: ConPrintL("DECEMBER 6 1847"); break;
		case 19: ConPrintL("DECEMBER 20 1847"); break;
		default: ConPrintL("WINTER"); break;
	}

	ConNewLine();
	ConNewLine();
}


static void PrintInventory(void){
	ConPrintL("TOTAL MILEAGE IS ");
	if(g.display_950_flag){
		ConPrintNum(950);
		g.display_950_flag = 0;
	} else{
		ConPrintNum(FxToIntFloor(g.mileage));
	}
	ConNewLine();

	ConPrintL("FOOD  BULLETS  CLOTHING  MISC. SUPP.  CASH");
	ConNewLine();
	ConPrintNum(FxToIntFloor(g.food));
	ConSpaces(6);
	ConPrintNum(FxToIntFloor(g.bullets));
	ConSpaces(5);
	ConPrintNum(FxToIntFloor(g.clothing));
	ConSpaces(7);
	ConPrintNum(FxToIntFloor(g.misc_supplies));
	ConSpaces(9);
	ConPrintNum(FxToIntFloor(g.cash));
	ConNewLine();
}


static void InitGame(void){
	u16 seed;

	memset(&g, 0, sizeof(g));
	g.fort_option_toggle = -1;

#if OREGON_USE_TRUE_RANDOM_SEED
	seed = GetTrueRandomSeed();
	if(seed == 0) seed = (u16)OREGON_FALLBACK_SEED;
#else
	seed = (u16)OREGON_FALLBACK_SEED;
#endif

	GetPrngNumber(seed);
	OregonSyncPrngVsyncMark();
	OregonSyncJoypadState();
	if(OregonScreenOffset > 3){
		OregonScreenOffset = 0;
	}
	g.death_reason = DEATH_NONE;
}


static void PrintInstructions(void){
	ConNewLine();
	ConNewLine();
	ConPrintL("THIS PROGRAM SIMULATES A TRIP OVER THE OREGON TRAIL FROM");
	ConNewLine();
	ConPrintL("INDEPENDENCE, MISSOURI TO OREGON CITY, OREGON IN 1847.");
	ConNewLine();
	ConPrintL("YOUR FAMILY OF FIVE WILL COVER THE 2040 MILE OREGON TRAIL");
	ConNewLine();
	ConPrintL("IN 5-6 MONTHS --- IF YOU MAKE IT ALIVE.");
	ConNewLine();
	ConNewLine();
	ConPrintL("YOU HAD SAVED $900 TO SPEND FOR THE TRIP, AND YOU'VE JUST");
	ConNewLine();
	ConPrintL("PAID $200 FOR A WAGON.");
	ConNewLine();
	ConPrintL("YOU WILL NEED TO SPEND THE REST OF YOUR MONEY ON THE");
	ConNewLine();
	ConPrintL("FOLLOWING ITEMS:");
	ConNewLine();
	ConNewLine();
	ConPrintL("OXEN - YOU CAN SPEND $200-$300 ON YOUR TEAM");
	ConNewLine();
	ConPrintL("THE MORE YOU SPEND, THE FASTER YOU'LL GO");
	ConNewLine();
	ConPrintL("BECAUSE YOU'LL HAVE BETTER ANIMALS");
	ConNewLine();
	ConNewLine();
	ConPrintL("FOOD - THE MORE YOU HAVE, THE LESS CHANCE THERE");
	ConNewLine();
	ConPrintL("IS OF GETTING SICK");
	ConNewLine();
	ConNewLine();
	ConPrintL("AMMUNITION - $1 BUYS A BELT OF 50 BULLETS");
	ConNewLine();
	ConPrintL("YOU WILL NEED BULLETS FOR ATTACKS BY ANIMALS");
	ConNewLine();
	ConPrintL("AND BANDITS, AND FOR HUNTING FOOD");
	ConNewLine();
	ConNewLine();
	ConPrintL("CLOTHING - THIS IS ESPECIALLY IMPORTANT FOR THE COLD");
	ConNewLine();
	ConPrintL("WEATHER YOU WILL ENCOUNTER WHEN CROSSING");
	ConNewLine();
	ConPrintL("THE MOUNTAINS");
	ConNewLine();
	ConNewLine();
	ConPrintL("MISCELLANEOUS SUPPLIES - THIS INCLUDES MEDICINE AND");
	ConNewLine();
	ConPrintL("OTHER THINGS YOU WILL NEED FOR SICKNESS");
	ConNewLine();
	ConPrintL("AND EMERGENCY REPAIRS");
	ConNewLine();
	ConNewLine();
	ConPrintL("YOU CAN SPEND ALL YOUR MONEY BEFORE YOU START YOUR TRIP -");
	ConNewLine();
	ConPrintL("OR YOU CAN SAVE SOME OF YOUR CASH TO SPEND AT FORTS ALONG");
	ConNewLine();
	ConPrintL("THE WAY WHEN YOU RUN LOW. HOWEVER, ITEMS COST MORE AT");
	ConNewLine();
	ConPrintL("THE FORTS. YOU CAN ALSO GO HUNTING ALONG THE WAY TO GET");
	ConNewLine();
	ConPrintL("MORE FOOD.");
	ConNewLine();
	ConNewLine();
	ConPrintL("WHENEVER YOU HAVE TO USE YOUR TRUSTY RIFLE ALONG THE WAY,");
	ConNewLine();
	ConPrintL("YOU WILL BE TOLD TO TYPE IN A WORD (ONE THAT SOUNDS LIKE A");
	ConNewLine();
	ConPrintL("GUN SHOT). THE FASTER YOU TYPE IN THAT WORD AND HIT THE");
	ConNewLine();
	ConPrintL("RETURN KEY, THE BETTER LUCK YOU'LL HAVE WITH YOUR GUN.");
	ConNewLine();
	ConNewLine();
	ConPrintL("AT EACH TURN, ALL ITEMS ARE SHOWN IN DOLLAR AMOUNTS");
	ConNewLine();
	ConPrintL("EXCEPT BULLETS");
	ConNewLine();
	ConPrintL("WHEN ASKED TO ENTER MONEY AMOUNTS, DON'T USE A $");
	ConNewLine();
	ConNewLine();
	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		ConPrintL("JOYPAD CONTROLS: LEFT/RIGHT MOVE DIGIT, UP/DOWN CHANGE");
		ConNewLine();
		ConPrintL("DIGIT, A OR START ACCEPT, B CLEARS CURRENT DIGIT.");
		ConNewLine();
		ConPrintL("SHOOTING: A=BANG  B=BLAM  LEFT=POW  RIGHT=WHAM");
		ConNewLine();
		ConNewLine();
	}
	ConPrintL("GOOD LUCK!!!");
	ConNewLine();
}


static void AskInstructions(void){
	char response_buf[8];

	ConPrintL("DO YOU NEED INSTRUCTIONS (YES/NO)");
	ConNewLine();
	ReadLine(response_buf, sizeof(response_buf), 1);
	StrToUpper(response_buf);

#if OREGON_SOURCE_INPUT_QUIRKS
	if(!StreqL(response_buf, "NO"))
#else
	if(StreqL(response_buf, "YES") || StreqL(response_buf, "Y"))
#endif
	{
		PrintInstructions();
	}
}


static void AskShotSkill(void){
	while(1){
		ConEnsureLinesAvailable(4);
		ConPrintL("HOW GOOD A SHOT ARE YOU WITH YOUR RIFLE?");
		ConNewLine();
		ConPrintL(" (1) ACE MARKSMAN,  (2) GOOD SHOT,  (3) FAIR TO MIDDLIN'");
		ConNewLine();
		ConPrintL(" (4) NEED MORE PRACTICE,  (5) SHAKY KNEES");
		ConNewLine();
		ConPrintL("ENTER ONE OF THE ABOVE -- ");
		g.shooting_skill = (u8)ReadIntRaw();

#if OREGON_SOURCE_INPUT_QUIRKS
		if(g.shooting_skill > 5){
			g.shooting_skill = 0;
		}
		break;
#else
		if(g.shooting_skill >= 1 && g.shooting_skill <= 5) break;
#endif
	}
}


static void DoInitialPurchases(void){
	while(1){
		ConNewLine();
		ConNewLine();

		while(1){
			ConPrintL("HOW MUCH DO YOU WANT TO SPEND ON YOUR OXEN TEAM ");
			g.oxen_spend = FxFromInt(ReadIntRaw());
			if(g.oxen_spend < FxFromInt(200)){
				ConPrintL("NOT ENOUGH");
				ConNewLine();
				continue;
			}
			if(g.oxen_spend > FxFromInt(300)){
				ConPrintL("TOO MUCH");
				ConNewLine();
				continue;
			}
			break;
		}

		while(1){
			ConPrintL("HOW MUCH DO YOU WANT TO SPEND ON FOOD ");
			g.food = FxFromInt(ReadIntRaw());
			if(g.food >= 0)
				break;
			ConPrintL("IMPOSSIBLE");
			ConNewLine();
		}

		while(1){
			ConPrintL("HOW MUCH DO YOU WANT TO SPEND ON AMMUNITION ");
			g.bullets = FxFromInt(ReadIntRaw());
			if(g.bullets >= 0)
				break;
			ConPrintL("IMPOSSIBLE");
			ConNewLine();
		}

		while(1){
			ConPrintL("HOW MUCH DO YOU WANT TO SPEND ON CLOTHING ");
			g.clothing = FxFromInt(ReadIntRaw());
			if(g.clothing >= 0)
				break;
			ConPrintL("IMPOSSIBLE");
			ConNewLine();
		}

		while(1){
			ConPrintL("HOW MUCH DO YOU WANT TO SPEND ON MISCELLANEOUS SUPPLIES ");
			g.misc_supplies = FxFromInt(ReadIntRaw());
			if(g.misc_supplies >= 0)
				break;
			ConPrintL("IMPOSSIBLE");
			ConNewLine();
		}

		g.cash = FxFromInt(700) - g.oxen_spend - g.food - g.bullets - g.clothing - g.misc_supplies;
		if(g.cash >= 0)
			break;

		ConPrintL("YOU OVERSPENT--YOU ONLY HAD $700 TO SPEND. BUY AGAIN");
		ConNewLine();
	}

	g.bullets = 50 * g.bullets;

	ConPrintL("AFTER ALL YOUR PURCHASES, YOU NOW HAVE ");
	ConPrintNum(FxToIntFloor(g.cash));
	ConPrintL(" DOLLARS LEFT");
	ConNewLine();
	ConNewLine();
	PrintDateTurn(0);
}


static void ShootSubroutine(void){
	char expected_word[6];
	char typed_word[16];
	const char *word_ptr;
	u32 elapsed_ticks;

	g.shot_word_index = (u8)(FxToIntFloor(RandFx(4)) + 1);
	word_ptr = (const char *)pgm_read_word(&shot_words[g.shot_word_index - 1]);
	strcpy_P(expected_word, (PGM_P)word_ptr);

	if(OregonInputMode == OREGON_INPUT_JOYPAD){
		u16 newly_pressed;
		u8 correct_pressed = 0;

		ConEnsureLinesAvailable(3);
		ConPrintL("PRESS ");
		ConPrint(expected_word);
		ConNewLine();
		ConPrintL("A=BANG  B=BLAM  LEFT=POW  RIGHT=WHAM");
		ConNewLine();

		ClearVsyncCounter();
		OregonSyncPrngVsyncMark();
		while(1){
			OregonAdvancePrngPerElapsedFrame();
			newly_pressed = OregonReadJoypadPressed();
			if(newly_pressed == 0){
				WaitVsync(1);
				continue;
			}

			if((newly_pressed & BTN_A) && g.shot_word_index == 1) correct_pressed = 1;
			if((newly_pressed & BTN_B) && g.shot_word_index == 2) correct_pressed = 1;
			if((newly_pressed & BTN_LEFT) && g.shot_word_index == 3) correct_pressed = 1;
			if((newly_pressed & BTN_RIGHT) && g.shot_word_index == 4) correct_pressed = 1;
			break;
		}

		elapsed_ticks = GetVsyncCounter();
		{
			u8 shot_frames_per_unit = OregonGetShotFramesPerUnit();
			if(shot_frames_per_unit > 1u){
				elapsed_ticks = (elapsed_ticks + (shot_frames_per_unit - 1u)) / shot_frames_per_unit;
			}
		}

		g.shot_time = (s16)elapsed_ticks;
		g.shot_time = (s16)(g.shot_time - (g.shooting_skill - 1));
		if(g.shot_time < 0) g.shot_time = 0;
		if(!correct_pressed){
			g.shot_time = 9;
		}
		ConNewLine();
		return;
	}

	ConEnsureLinesAvailable(2);
	ConPrintL("TYPE ");
	ConPrint(expected_word);
	ConNewLine();

	ClearVsyncCounter();
	OregonSyncPrngVsyncMark();
	ReadLine(typed_word, sizeof(typed_word), 1);
	elapsed_ticks = GetVsyncCounter();

	u8 shot_frames_per_unit = OregonGetShotFramesPerUnit();
	if(shot_frames_per_unit > 1u){
		elapsed_ticks = (elapsed_ticks + (shot_frames_per_unit - 1u)) / shot_frames_per_unit;
	}

	g.shot_time = (s16)elapsed_ticks;
	g.shot_time = (s16)(g.shot_time - (g.shooting_skill - 1));
	if(g.shot_time < 0) g.shot_time = 0;

	StrToUpper(typed_word);
	if(!Streq(typed_word, expected_word)){
		g.shot_time = 9;
	}

	ConNewLine();
}


static void DoNoDoctorDeath(void){
	g.cash = 0;
	ConPrintL("YOU CAN'T AFFORD A DOCTOR");
	ConNewLine();
	if(g.injury_flag)
	{
		ConPrintL("YOU DIED OF INJURIES");
		MarkDead(DEATH_INJURY);
	}else{
		ConPrintL("YOU DIED OF PNEUMONIA");
		MarkDead(DEATH_PNEUMONIA);
	}
	ConNewLine();
}


static void DoNoMedicineDeath(void){
	ConPrintL("YOU RAN OUT OF MEDICAL SUPPLIES");
	ConNewLine();
	if(g.injury_flag){
		ConPrintL("YOU DIED OF INJURIES");
		MarkDead(DEATH_INJURY);
	}else{
		ConPrintL("YOU DIED OF PNEUMONIA");
		MarkDead(DEATH_PNEUMONIA);
	}
	ConNewLine();
}


static void DoIllness(void){
	u8 illness_roll;

	illness_roll = RandPct();
	if(illness_roll < (u8)(10 + 35 * (g.eating_choice - 1))){
		ConPrintL("MILD ILLNESS---MEDICINE USED");
		ConNewLine();
		g.mileage -= FxFromInt(5);
		g.misc_supplies -= FxFromInt(2);
	}else{
		u8 severe_limit;

		if(g.eating_choice == 1)
			severe_limit = 60;
		else if(g.eating_choice == 2)
			severe_limit = 90;
		else
			severe_limit = 97;

		illness_roll = RandPct();
		if(illness_roll < severe_limit){
			ConPrintL("BAD ILLNESS---MEDICINE USED");
			ConNewLine();
			g.mileage -= FxFromInt(5);
			g.misc_supplies -= FxFromInt(5);
		}else{
			ConPrintL("SERIOUS ILLNESS---");
			ConNewLine();
			ConPrintL("YOU MUST STOP FOR MEDICAL ATTENTION");
			ConNewLine();
			g.misc_supplies -= FxFromInt(10);
			g.illness_flag = 1;
		}
	}

	if(g.misc_supplies < 0){
		DoNoMedicineDeath();
	}
}


static fx32 ReadFortSpend_P(PGM_P label){
	fx32 requested_spend;

	ConPrint_P(label);
	requested_spend = FxFromInt(ReadIntRaw());
	if(requested_spend < 0) return 0;

	g.cash -= requested_spend;
	if(g.cash >= 0){
		return requested_spend;
	}

	ConPrintL("YOU DON'T HAVE THAT MUCH--KEEP YOUR SPENDING DOWN");
	ConNewLine();
	ConPrintL("YOU MISS YOUR CHANCE TO SPEND ON THAT ITEM");
	ConNewLine();
	g.cash += requested_spend;
	return 0;
}


static void DoFort(void){
	fx32 requested_spend;

	ConPrintL("ENTER WHAT YOU WISH TO SPEND ON THE FOLLOWING");
	ConNewLine();

	requested_spend = ReadFortSpendL("FOOD ");
	g.food += (2 * requested_spend) / 3;

	requested_spend = ReadFortSpendL("AMMUNITION ");
	g.bullets += (100 * requested_spend) / 3;

	requested_spend = ReadFortSpendL("CLOTHING ");
	g.clothing += (2 * requested_spend) / 3;

	requested_spend = ReadFortSpendL("MISCELLANEOUS SUPPLIES ");
	g.misc_supplies += (2 * requested_spend) / 3;

	g.mileage -= FxFromInt(45);
}


static void DoHunt(void){
	if(g.bullets <= FxFromInt(39)){
		ConPrintL("TOUGH---YOU NEED MORE BULLETS TO GO HUNTING");
		ConNewLine();
		return;
	}

	g.mileage -= FxFromInt(45);
	ShootSubroutine();

	if(g.shot_time <= 1){
		ConPrintL("RIGHT BETWEEN THE EYES---YOU GOT A BIG ONE!!!!");
		ConNewLine();
		ConPrintL("FULL BELLIES TONIGHT!");
		ConNewLine();
		g.food += FxFromInt(52) + RandFx(6);
		g.bullets -= FxFromInt(10) + RandFx(4);
		return;
	}

	if((s16)(13 * g.shot_time) > RandPct()){
		ConPrintL("YOU MISSED---AND YOUR DINNER GOT AWAY.....");
		ConNewLine();
		return;
	}

	g.food += FxFromInt((s16)(48 - 2 * g.shot_time));
	ConPrintL("NICE SHOT--RIGHT ON TARGET--GOOD EATIN' TONIGHT!!");
	ConNewLine();
	g.bullets -= FxFromInt((s16)(10 + 3 * g.shot_time));
}


static void RidersPhase(void){
	s32 distance_term;
	s32 hostility_numerator;
	s32 hostility_denominator;

	distance_term = (s32)(FxToIntFloor(g.mileage) / 100 - 4);
	hostility_numerator = distance_term * distance_term + 72;
	hostility_denominator = distance_term * distance_term + 12;

	if((((s32)FxToIntFloor(RandFx(10))) * hostility_numerator) / hostility_denominator > 1){
		return;
	}

	ConPrintL("RIDERS AHEAD. THEY ");
	g.riders_non_hostile_flag = 0;

	if(!RandPctLess(80)){
		ConPrintL("DON'T ");
		g.riders_non_hostile_flag = 1;
	}

	ConPrintL("LOOK HOSTILE");
	ConNewLine();
	ConPrintL("TACTICS");
	ConNewLine();
	ConPrintL("(1) RUN  (2) ATTACK  (3) CONTINUE  (4) CIRCLE WAGONS");
	ConNewLine();

	if(RandPctLess(20)){
		g.riders_non_hostile_flag = (u8)(1 - g.riders_non_hostile_flag);
	}

	while(1){
		g.rider_tactic = (u8)ReadIntRaw();
		if(g.rider_tactic >= 1 && g.rider_tactic <= 4) break;
	}

	if(g.riders_non_hostile_flag == 0){
		if(g.rider_tactic == 1){
			g.mileage += FxFromInt(20);
			g.misc_supplies -= FxFromInt(15);
			g.bullets -= FxFromInt(150);
			g.oxen_spend -= FxFromInt(40);
		}else if(g.rider_tactic == 2){
			ShootSubroutine();
			g.bullets -= FxFromInt((s16)(g.shot_time * 40 + 80));
			if(g.shot_time <= 1){
				ConPrintL("NICE SHOOTING---YOU DROVE THEM OFF");
				ConNewLine();
			}else if(g.shot_time > 4){
				ConPrintL("LOUSY SHOT---YOU GOT KNIFED");
				ConNewLine();
				g.injury_flag = 1;
				ConPrintL("YOU HAVE TO SEE OL' DOC BLANCHARD");
				ConNewLine();
			}else{
				ConPrintL("KINDA SLOW WITH YOUR COLT .45");
				ConNewLine();
			}
		}else if(g.rider_tactic == 3){
			if(RandPctLess(80)){
				g.bullets -= FxFromInt(150);
				g.misc_supplies -= FxFromInt(15);
			}else{
				ConPrintL("THEY DID NOT ATTACK");
				ConNewLine();
				return;
			}
		}else{
			ShootSubroutine();
			g.bullets -= FxFromInt((s16)(g.shot_time * 30 + 80));
			g.mileage -= FxFromInt(25);

			if(g.shot_time <= 1){
				ConPrintL("NICE SHOOTING---YOU DROVE THEM OFF");
				ConNewLine();
			}else if(g.shot_time > 4){
				ConPrintL("LOUSY SHOT---YOU GOT KNIFED");
				ConNewLine();
				g.injury_flag = 1;
				ConPrintL("YOU HAVE TO SEE OL' DOC BLANCHARD");
				ConNewLine();
			}else{
				ConPrintL("KINDA SLOW WITH YOUR COLT .45");
				ConNewLine();
			}
		}

		ConPrintL("RIDERS WERE HOSTILE--CHECK FOR LOSSES");
		ConNewLine();

		if(g.bullets < 0){
			ConPrintL("YOU RAN OUT OF BULLETS AND GOT MASSACRED BY THE RIDERS");
			ConNewLine();
			MarkDead(DEATH_MASSACRE);
		}
	}else{
		if(g.rider_tactic == 1){
			g.mileage += FxFromInt(15);
			g.oxen_spend -= FxFromInt(10);
		}else if(g.rider_tactic == 2){
			g.mileage -= FxFromInt(5);
			g.bullets -= FxFromInt(100);
		}else if(g.rider_tactic == 4){
			g.mileage -= FxFromInt(20);
		}

		ConPrintL("RIDERS WERE FRIENDLY, BUT CHECK FOR POSSIBLE LOSSES");
		ConNewLine();
	}
}


static void DoEvent(void){
	u8 event_id = 16;
	u8 threshold_index;
	u8 event_roll;

	g.event_roll_index = 0;
	event_roll = RandPct();

	for(threshold_index = 0; threshold_index < 15; ++threshold_index){
		if(event_roll <= pgm_read_byte(&event_thresholds[threshold_index])){
			event_id = (u8)(threshold_index + 1);
			break;
		}
	}

	switch(event_id){
		case 1:
			ConPrintL("WAGON BREAKS DOWN--LOSE TIME AND SUPPLIES FIXING IT");
			ConNewLine();
			g.mileage -= FxFromInt(15) + RandFx(5);
			g.misc_supplies -= FxFromInt(8);
			break;

		case 2:
			ConPrintL("OX INJURES LEG---SLOWS YOU DOWN REST OF TRIP");
			ConNewLine();
			g.mileage -= FxFromInt(25);
			g.oxen_spend -= FxFromInt(20);
			break;

		case 3:
			ConPrintL("BAD LUCK---YOUR DAUGHTER BROKE HER ARM");
			ConNewLine();
			ConPrintL("YOU HAD TO STOP AND USE SUPPLIES TO MAKE A SLING");
			ConNewLine();
			g.mileage -= FxFromInt(5) + RandFx(4);
			g.misc_supplies -= FxFromInt(2) + RandFx(3);
			break;

		case 4:
			ConPrintL("OX WANDERS OFF---SPEND TIME LOOKING FOR IT");
			ConNewLine();
			g.mileage -= FxFromInt(17);
			break;

		case 5:
			ConPrintL("YOUR SON GETS LOST---SPEND HALF THE DAY LOOKING FOR HIM");
			ConNewLine();
			g.mileage -= FxFromInt(10);
			break;

		case 6:
			ConPrintL("UNSAFE WATER--LOSE TIME LOOKING FOR CLEAN SPRING");
			ConNewLine();
			g.mileage -= FxFromInt(2) + RandFx(10);
			break;

		case 7:
			if(g.mileage > FxFromInt(950)){
				ConPrintL("COLD WEATHER---BRRRRRRR!---YOU ");
				if(g.clothing <= (FxFromInt(22) + RandFx(4))){
					ConPrintL("DON'T ");
					g.insufficient_clothing_flag = 1;
				}else{
					g.insufficient_clothing_flag = 0;
				}
				ConPrintL("HAVE ENOUGH CLOTHING TO KEEP YOU WARM");
				ConNewLine();
				if(g.insufficient_clothing_flag){
					DoIllness();
				}
			}else{
				ConPrintL("HEAVY RAINS---TIME AND SUPPLIES LOST");
				ConNewLine();
				g.food -= FxFromInt(10);
				g.bullets -= FxFromInt(500);
				g.misc_supplies -= FxFromInt(15);
				g.mileage -= FxFromInt(5) + RandFx(10);
			}
			break;

		case 8:
			ConPrintL("BANDITS ATTACK");
			ConNewLine();
			ShootSubroutine();
			g.bullets -= FxFromInt((s16)(20 * g.shot_time));
			if(g.bullets < 0){
				ConPrintL("YOU RAN OUT OF BULLETS---THEY GET LOTS OF CASH");
				ConNewLine();
				g.cash /= 3;
			}
#if OREGON_SOURCE_BUG_COMPAT
			else{
				ConPrintL("YOU GOT SHOT IN THE LEG AND THEY TOOK ONE OF YOUR OXEN");
				ConNewLine();
				g.injury_flag = 1;
				ConPrintL("BETTER HAVE A DOC LOOK AT YOUR WOUND");
				ConNewLine();
				g.misc_supplies -= FxFromInt(5);
				g.oxen_spend -= FxFromInt(20);
			}
#else
			else if(g.shot_time > 1){
				ConPrintL("YOU GOT SHOT IN THE LEG AND THEY TOOK ONE OF YOUR OXEN");
				ConNewLine();
				g.injury_flag = 1;
				ConPrintL("BETTER HAVE A DOC LOOK AT YOUR WOUND");
				ConNewLine();
				g.misc_supplies -= FxFromInt(5);
				g.oxen_spend -= FxFromInt(20);
			}else{
				ConPrintL("QUICKEST DRAW OUTSIDE OF DODGE CITY!!!");
				ConNewLine();
				ConPrintL("YOU GOT 'EM!");
				ConNewLine();
			}
#endif
			break;

		case 9:
			ConPrintL("THERE WAS A FIRE IN YOUR WAGON--FOOD AND SUPPLIES DAMAGE");
			ConNewLine();
			g.food -= FxFromInt(40);
			g.bullets -= FxFromInt(400);
			g.misc_supplies -= RandFx(8) + FxFromInt(3);
			g.mileage -= FxFromInt(15);
			break;

		case 10:
			ConPrintL("LOSE YOUR WAY IN HEAVY FOG---TIME IS LOST");
			ConNewLine();
			g.mileage -= FxFromInt(10) + RandFx(5);
			break;

		case 11:
			ConPrintL("YOU KILLED A POISONOUS SNAKE AFTER IT BIT YOU");
			ConNewLine();
			g.bullets -= FxFromInt(10);
			g.misc_supplies -= FxFromInt(5);
			if(g.misc_supplies < 0){
				ConPrintL("YOU DIE OF SNAKEBITE SINCE YOU HAVE NO MEDICINE");
				ConNewLine();
				MarkDead(DEATH_SNAKEBITE);
			}
			break;

		case 12:
			ConPrintL("WAGON GETS SWAMPED FORDING RIVER--LOSE FOOD AND CLOTHES");
			ConNewLine();
			g.food -= FxFromInt(30);
			g.clothing -= FxFromInt(20);
			g.mileage -= FxFromInt(20) + RandFx(20);
			break;

		case 13:
			ConPrintL("WILD ANIMALS ATTACK!");
			ConNewLine();
			ShootSubroutine();
			if(g.bullets <= FxFromInt(39)){
				ConPrintL("YOU WERE TOO LOW ON BULLETS--");
				ConNewLine();
				ConPrintL("THE WOLVES OVERPOWERED YOU");
				ConNewLine();
				g.injury_flag = 1;
				MarkDead(DEATH_INJURY);
			}else{
#if OREGON_SOURCE_BUG_COMPAT
				ConPrintL("NICE SHOOTIN' PARTNER---THEY DIDN'T GET MUCH");
				ConNewLine();
#else
				if(g.shot_time > 2){
					ConPrintL("SLOW ON THE DRAW---THEY GOT AT YOUR FOOD AND CLOTHES");
					ConNewLine();
				}else{
					ConPrintL("NICE SHOOTIN' PARTNER---THEY DIDN'T GET MUCH");
					ConNewLine();
				}
#endif

				g.bullets -= FxFromInt((s16)(20 * g.shot_time));
				g.clothing -= FxFromInt((s16)(4 * g.shot_time));
				g.food -= FxFromInt((s16)(8 * g.shot_time));
			}
			break;

		case 14:
			ConPrintL("HAIL STORM---SUPPLIES DAMAGED");
			ConNewLine();
			g.mileage -= FxFromInt(5) + RandFx(10);
			g.bullets -= FxFromInt(200);
			g.misc_supplies -= FxFromInt(4) + RandFx(3);
			break;

		case 15:
			if(g.eating_choice == 1){
				DoIllness();
			}else if(g.eating_choice == 3){
				if(RandPctLess(50)){
					DoIllness();
				}
			}else{
				if(!RandPctLess(25)){
					DoIllness();
				}
			}
			break;

		default:
			ConPrintL("HELPFUL INDIANS SHOW YOU WHERE TO FIND MORE FOOD");
			ConNewLine();
			g.food += FxFromInt(14);
			break;
	}
}


static void DoMountains(void){
	s32 distance_term;
	s32 skip_mountains_threshold;

	if(g.mileage <= FxFromInt(950))
		return;

	distance_term = (s32)(FxToIntFloor(g.mileage) / 100 - 15);
	skip_mountains_threshold = 9 - (((distance_term * distance_term) + 72) / ((distance_term * distance_term) + 12));

	if((s32)FxToIntFloor(RandFx(10)) > skip_mountains_threshold){
		goto south_pass;
	}

	ConPrintL("RUGGED MOUNTAINS");
	ConNewLine();

	if(RandPctLess(10)){
		ConPrintL("YOU GOT LOST---LOSE VALUABLE TIME TRYING TO FIND TRAIL!");
		ConNewLine();
		g.mileage -= FxFromInt(60);
		goto south_pass;
	}

	if(RandPctLess(11)){
		ConPrintL("WAGON DAMAGED!---LOSE TIME AND SUPPLIES");
		ConNewLine();
		g.misc_supplies -= FxFromInt(5);
		g.bullets -= FxFromInt(200);
		g.mileage -= FxFromInt(20) + RandFx(30);
		goto south_pass;
	}

	ConPrintL("THE GOING GETS SLOW");
	ConNewLine();
	g.mileage -= FxFromInt(45) + RandFx(50);

south_pass:
	if(g.south_pass_cleared == 0){
		g.south_pass_cleared = 1;
		if(!RandPctLess(80)){
			ConPrintL("YOU MADE IT SAFELY THROUGH SOUTH PASS--NO SNOW");
			ConNewLine();
		}else{
			goto blizzard;
		}
	}

	if(g.mileage >= FxFromInt(1700) && g.blue_mountains_cleared == 0){
		g.blue_mountains_cleared = 1;
		if(RandPctLess(70)){
			goto blizzard;
		}
	}

	if(g.mileage <= FxFromInt(950)){
		g.display_950_flag = 1;
	}

	return;

blizzard:
	ConPrintL("BLIZZARD IN MOUNTAIN PASS--TIME AND SUPPLIES LOST");
	ConNewLine();
	g.blizzard_flag = 1;
	g.food -= FxFromInt(25);
	g.misc_supplies -= FxFromInt(10);
	g.bullets -= FxFromInt(300);
	g.mileage -= FxFromInt(30) + RandFx(40);

	if(g.clothing < (FxFromInt(18) + RandFx(2))){
		DoIllness();
	}

	if(!g.game_over && g.mileage <= FxFromInt(950)){
		g.display_950_flag = 1;
	}
}


static void TurnStart(void){
	if(g.food < 0)
		g.food = 0;
	if(g.bullets < 0)
		g.bullets = 0;
	if(g.clothing < 0)
		g.clothing = 0;
	if(g.misc_supplies < 0)
		g.misc_supplies = 0;

	g.food = FxFloorValue(g.food);
	g.bullets = FxFloorValue(g.bullets);
	g.clothing = FxFloorValue(g.clothing);
	g.misc_supplies = FxFloorValue(g.misc_supplies);
	g.cash = FxFloorValue(g.cash);
	g.mileage = FxFloorValue(g.mileage);

	if(g.food < FxFromInt(13)){
		ConPrintL("YOU'D BETTER DO SOME HUNTING OR BUY FOOD AND SOON!!!!");
		ConNewLine();
	}

	g.prev_mileage = g.mileage;

	if(g.illness_flag || g.injury_flag){
		g.cash -= FxFromInt(20);
		if(g.cash < 0){
			DoNoDoctorDeath();
			return;
		}

		ConPrintL("DOCTOR'S BILL IS $20");
		ConNewLine();
		g.injury_flag = 0;
		g.illness_flag = 0;
	}

	PrintInventory();
}


static u8 AskAction(void){
	s16 menu_choice;

	while(1){
		if(g.fort_option_toggle == -1){
			ConPrintL("DO YOU WANT TO (1) HUNT, OR (2) CONTINUE");
			ConNewLine();
					menu_choice = ReadIntRaw();
			if(menu_choice == 1){
				if(g.bullets <= FxFromInt(39)){
					ConPrintL("TOUGH---YOU NEED MORE BULLETS TO GO HUNTING");
					ConNewLine();
					continue;
				}
				g.fort_option_toggle = (s8)(-g.fort_option_toggle);
				return 2;
			}

			g.fort_option_toggle = (s8)(-g.fort_option_toggle);
			return 3;
		}else{
			ConPrintL("DO YOU WANT TO (1) STOP AT THE NEXT FORT, (2) HUNT, OR (3) CONTINUE");
			ConNewLine();
			menu_choice = ReadIntRaw();

			if(menu_choice < 1 || menu_choice > 3)
				menu_choice = 3;
			if(menu_choice == 2 && g.bullets <= FxFromInt(39)){
				ConPrintL("TOUGH---YOU NEED MORE BULLETS TO GO HUNTING");
				ConNewLine();
				continue;
			}

			g.fort_option_toggle = (s8)(-g.fort_option_toggle);
			return (u8)menu_choice;
		}
	}
}


static void AskEat(void){
	while(1){
		ConPrintL("DO YOU WANT TO EAT (1) POORLY  (2) MODERATELY");
		ConNewLine();
		ConPrintL("OR (3) WELL ");
		g.eating_choice = (u8)ReadIntRaw();

		if(g.eating_choice < 1 || g.eating_choice > 3)
			continue;

		g.food -= FxFromInt((s16)(8 + 5 * g.eating_choice));
		if(g.food >= 0)
			return;

		g.food += FxFromInt((s16)(8 + 5 * g.eating_choice));
		ConPrintL("YOU CAN'T EAT THAT WELL");
		ConNewLine();
	}
}


static void AdvanceDate(void){
	if(g.mileage >= FxFromInt(2040))
		return;

	++g.turn_index;
	ConNewLine();

	if(g.turn_index > 19){
		ConPrintL("YOU HAVE BEEN ON THE TRAIL TOO LONG  ------");
		ConNewLine();
		ConPrintL("YOUR FAMILY DIES IN THE FIRST BLIZZARD OF WINTER");
		ConNewLine();
		MarkDead(DEATH_WINTER);
		return;
	}

	PrintDateTurn(g.turn_index);
}


static void DoWinSequence(void){
	u8 arrival_day_of_week;
	s16 arrival_day_index;
	fx32 travel_distance_denominator;
	fx32 arrival_distance_numerator;
	s32 arrival_fraction14;
	fx32 final_food_refund;

	g.won = 1;

	travel_distance_denominator = g.mileage - g.prev_mileage;
	arrival_distance_numerator = FxFromInt(2040) - g.prev_mileage;
	arrival_fraction14 = 0;
	final_food_refund = 0;

	if(travel_distance_denominator > 0){
		if(arrival_distance_numerator < 0) arrival_distance_numerator = 0;
		if(arrival_distance_numerator > travel_distance_denominator) arrival_distance_numerator = travel_distance_denominator;

		final_food_refund = (fx32)(((s64)(travel_distance_denominator - arrival_distance_numerator) * (s64)FxFromInt((s16)(8 + 5 * g.eating_choice))) / (s64)travel_distance_denominator);
		arrival_fraction14 = (s32)(((s64)arrival_distance_numerator * 14) / (s64)travel_distance_denominator);
		if(arrival_fraction14 < 0) arrival_fraction14 = 0;
		if(arrival_fraction14 > 14) arrival_fraction14 = 14;
	}

	g.food += final_food_refund;
	g.final_turn_fraction = (u8)arrival_fraction14;

	ConNewLine();
	ConPrintL("YOU FINALLY ARRIVED AT OREGON CITY");
	ConNewLine();
	ConPrintL("AFTER 2040 LONG MILES---HOORAY!!!!!");
	ConNewLine();
	ConPrintL("A REAL PIONEER!");
	ConNewLine();
	ConNewLine();

	arrival_day_index = (s16)(g.turn_index * 14 + g.final_turn_fraction);
	arrival_day_of_week = (u8)(g.final_turn_fraction + 1);
	if(arrival_day_of_week >= 8)
		arrival_day_of_week = (u8)(arrival_day_of_week - 7);

	switch(arrival_day_of_week){
		case 1: ConPrintL("MONDAY "); break;
		case 2: ConPrintL("TUESDAY "); break;
		case 3: ConPrintL("WEDNESDAY "); break;
		case 4: ConPrintL("THURSDAY "); break;
		case 5: ConPrintL("FRIDAY "); break;
		case 6: ConPrintL("SATURDAY "); break;
		default: ConPrintL("SUNDAY "); break;
	}

	if(arrival_day_index <= 124){
		ConPrintL("JULY ");
		ConPrintNum((s16)(arrival_day_index - 93));
		ConPrintL(" 1847");
	}else if(arrival_day_index <= 155){
		ConPrintL("AUGUST ");
		ConPrintNum((s16)(arrival_day_index - 124));
		ConPrintL(" 1847");
	}else if(arrival_day_index <= 185){
		ConPrintL("SEPTEMBER ");
		ConPrintNum((s16)(arrival_day_index - 155));
		ConPrintL(" 1847");
	}else if(arrival_day_index <= 216){
		ConPrintL("OCTOBER ");
		ConPrintNum((s16)(arrival_day_index - 185));
		ConPrintL(" 1847");
	}else if(arrival_day_index <= 246){
		ConPrintL("NOVEMBER ");
		ConPrintNum((s16)(arrival_day_index - 216));
		ConPrintL(" 1847");
	}else{
		ConPrintL("DECEMBER ");
		ConPrintNum((s16)(arrival_day_index - 246));
		ConPrintL(" 1847");
	}

	ConNewLine();
	ConNewLine();

	if(g.bullets < 0)
		g.bullets = 0;
	if(g.clothing < 0)
		g.clothing = 0;
	if(g.misc_supplies < 0)
		g.misc_supplies = 0;
	if(g.cash < 0)
		g.cash = 0;
	if(g.food < 0)
		g.food = 0;

	ConPrintL("FOOD  BULLETS  CLOTHING  MISC. SUPP.  CASH");
	ConNewLine();
	ConPrintNum(FxToIntFloor(g.food));
	ConSpaces(6);
	ConPrintNum(FxToIntFloor(g.bullets));
	ConSpaces(5);
	ConPrintNum(FxToIntFloor(g.clothing));
	ConSpaces(7);
	ConPrintNum(FxToIntFloor(g.misc_supplies));
	ConSpaces(9);
	ConPrintNum(FxToIntFloor(g.cash));
	ConNewLine();
	ConNewLine();

	PrintCenteredL("PRESIDENT JAMES K. POLK SENDS YOU HIS", 11);
	PrintCenteredL("HEARTIEST CONGRATULATIONS", 17);
	ConNewLine();
	PrintCenteredL("AND WISHES YOU A PROSPEROUS LIFE AHEAD", 11);
	ConNewLine();
	PrintCenteredL("AT YOUR NEW HOME", 22);
}


static void DoDeathFormality(void){
	char buf[16];

	ConNewLine();
	ConPrintL("DUE TO YOUR UNFORTUNATE SITUATION, THERE ARE A FEW");
	ConNewLine();
	ConPrintL("FORMALITIES WE MUST GO THROUGH");
	ConNewLine();
	ConNewLine();

	ConPrintL("WOULD YOU LIKE A MINISTER?");
	ConNewLine();
	ReadLine(buf, sizeof(buf), 1);

	ConPrintL("WOULD YOU LIKE A FANCY FUNERAL?");
	ConNewLine();
	ReadLine(buf, sizeof(buf), 1);

	ConPrintL("WOULD YOU LIKE US TO INFORM YOUR NEXT OF KIN?");
	ConNewLine();
	ReadLine(buf, sizeof(buf), 1);
	StrToUpper(buf);

#if OREGON_SOURCE_INPUT_QUIRKS
	if(StreqL(buf, "YES"))
#else
	if(StreqL(buf, "YES") || StreqL(buf, "Y"))
#endif
	{
		ConPrintL("THAT WILL BE $4.50 FOR THE TELEGRAPH CHARGE.");
		ConNewLine();
		ConNewLine();
	}else{
		ConPrintL("BUT YOUR AUNT SADIE IN ST. LOUIS IS REALLY WORRIED ABOUT YOU");
		ConNewLine();
		ConNewLine();
	}

	ConPrintL("WE THANK YOU FOR THIS INFORMATION AND WE ARE SORRY YOU");
	ConNewLine();
	ConPrintL("DIDN'T MAKE IT TO THE GREAT TERRITORY OF OREGON");
	ConNewLine();
	ConPrintL("BETTER LUCK NEXT TIME");
	ConNewLine();
	ConNewLine();
	ConNewLine();

	PrintCenteredL("SINCERELY", 30);
	ConNewLine();
	PrintCenteredL("THE OREGON CITY CHAMBER OF COMMERCE", 17);
}


static void PlayOneTurn(void){
	u8 action_choice;

	if(g.game_over || g.won)
		return;

	TurnStart();
	if(g.game_over || g.won)
		return;

	action_choice = AskAction();

	if(action_choice == 1)
		DoFort();
	else if(action_choice == 2)
		DoHunt();

	if(g.food < FxFromInt(13)){
		ConPrintL("YOU RAN OUT OF FOOD AND STARVED TO DEATH");
		ConNewLine();
		MarkDead(DEATH_STARVATION);
		return;
	}

	AskEat();

	g.mileage += FxFromInt(200) + ((g.oxen_spend - FxFromInt(220)) / 5) + RandFx(10);
	g.blizzard_flag = 0;
	g.insufficient_clothing_flag = 0;

	RidersPhase();
	if(g.game_over || g.won) return;

	DoEvent();
	if(g.game_over || g.won) return;

	DoMountains();
	if(g.game_over || g.won) return;

	if(g.mileage >= FxFromInt(2040)){
		DoWinSequence();
		return;
	}

	AdvanceDate();
}


int main(void){
	while(1){
		ConClear();

		InitGame();
		SelectScreenOffset();
		SelectInputMode();
		AskInstructions();
		ConNewLine();
		ConNewLine();
		AskShotSkill();
		DoInitialPurchases();

		while(!g.game_over && !g.won){
			PlayOneTurn();
		}

		if(g.game_over && !g.won){
			DoDeathFormality();
		}

		ConNewLine();
		if(OregonInputMode == OREGON_INPUT_JOYPAD){
			ConPrintL("-= PRESS A OR START TO PLAY AGAIN =-");
		}else{
			ConPrintL("-= PRESS ENTER TO PLAY AGAIN =-");
		}
		ConNewLine();
		WaitForEnter();
	}
}