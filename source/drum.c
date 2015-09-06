/*
 * GBA DRUM SEQUENCER
 * 
 * rot / tolm dyn         2015
 * 
 * borrows ideas and code from
 * dinamise by Avelino Herrera Morales 	
 * 								(code)  
 * reboy by lsl/		
 * 					 		 (concept)
 * lsdj	by j.kotlinsky		
 * 				           (interface)
 * 
 * 
 *  This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *  
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 * 
 * 
*/

#include <gba.h>
#include <maxmod.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "soundbank.h"
#include "soundbank_bin.h"
#include "r6502_portfont_bin.h"

#include "include.h"
#include "palette.h"
#include "help.h"


#define MAPADDRESS		MAP_BASE_ADR(31)	// our base map address
#define SCREEN_BUFFER  ((u16 *) 0x6000000)

#define MAX_COLUMNS	16
#define MAX_ROWS	12

//screen modes

#define SCREEN_PATTERN 	0
#define SCREEN_SONG		1
#define SCREEN_SETTINGS	2
#define SCREEN_SAMPLES	3
#define SCREEN_HELP     4

//play

#define PLAY_STOPPED	0
#define PLAY_PATTERN 	1 // loop a pattern
#define PLAY_SONG		2 // no looping
#define PLAY_LIVE 		3 // loop 'chains' of patterns

#define NO_PATTERN 255

u8 play = PLAY_STOPPED;
int play_next = NO_PATTERN;

//loop modes
#define SONG_MODE		0
#define LIVE_MODE		1

const char *loop_mode_text[] = { "SONG", "LIVE"};

//sync modes

#define SYNC_NONE	0
#define SYNC_SLAVE	1 		//lsdj serial
#define SYNC_MASTER 2 		//lsdj serial
#define SYNC_TRIG_SLAVE 3	//pulses
#define SYNC_TRIG_MASTER 4 	//pulses
#define SYNC_MIDI	5 		//midi clock in

const char *sync_mode_text[] = { "NONE", "SLAVE", "MASTER", "NANO SLAVE","NANO MASTER","MIDI" };

//menu opts

#define MENU_ACTION 	0
#define MENU_LEFT 		1
#define MENU_RIGHT 		2
#define MENU_UP 		3
#define MENU_DOWN 		4

unsigned int current_screen;

unsigned int cursor_x;
unsigned int cursor_y;

unsigned int xmod = 0;
unsigned int ymod = 3;
unsigned int draw_flag = 0;

unsigned int last_xmod=0;
unsigned int last_column=0;

int current_column = -1;

mm_sfxhand active_sounds[MAX_ROWS];

const int pitch_table[] = { 512,542, 574, 608, 645, 683, 724, 767, 812, 861, 912, 966, 
	1024, 1084, 1149, 1217, 1290, 1366, 1448, 1534, 1625, 1722, 1824, 1933, 2048 };

const char note_table[] = {'C', 'd', 'D', 'e', 'E', 'F', 'g', 'G', 'a', 'A', 'b', 'B', 
						   'C', 'd', 'D', 'e', 'E', 'F', 'g', 'G', 'a', 'A', 'b', 'B', 'C'};

const char *pan_string[] = {"L  ", "L+R", "  R"};

#define CHANNEL_SAMPLE 0
#define CHANNEL_VOLUME 1
#define CHANNEL_PITCH  2
#define CHANNEL_PAN    3

#define PAN_CENTER 128
#define PAN_LEFT 0
#define PAN_RIGHT 255
#define PAN_OFF 1						   

#define STATUS_STOPPED			0
#define STATUS_PLAYING_PATTERN	1
#define STATUS_PLAYING_SONG		2

#define STATUS_SYNC_STOPPED		3
#define STATUS_SYNC_PATTERN		4
#define STATUS_SYNC_SONG		5

#define STATUS_SAVE				6
#define STATUS_LOAD				7
#define STATUS_CLEAR			8

int serial_ticks = 0;


char *status_text[] = { "            ", "PATTERN PLAY", "SONG PLAY   ", "SYNC WAIT   ", 
		"SYNC PATTERN", "SYNC SONG   ", "SAVED OK    ", "LOADED OK   ", "CLEARED     "};

unsigned int status = 0;

//Timing stuff

#define MIN_BPM 30
#define MAX_BPM 300

int current_ticks = 0;

#define TICK 0
#define TOCK 1

int tick_or_tock = 0;
int millis_between_ticks;

int ticks;
int tocks;

#define REG_TM2D *(vu16*)0x4000108 		//4000108h - TM2CNT_L - Timer 2 Counter/Reload (R/W)
#define REG_TM2C *(vu16*)0x400010A 		//400010Ah - TM2CNT_H - Timer 2 Control (R/W)

#define REG_TM3D *(vu16*)0x400010C 		//400010Ch - TM3CNT_L - Timer 3 Counter/Reload (R/W)
#define REG_TM3C *(vu16*)0x400010E   	//400010Eh - TM3CNT_H - Timer 3 Control (R/W)

#define TM_ENABLE 		(vu16)0x80
#define TM_CASCADE 		(vu16)0x4
#define TM_IRQ			(vu16)BIT(6)

#define TM_FREQ_1 		(vu16)0x0
#define TM_FREQ_64 		(vu16)0x1
#define TM_FREQ_256		(vu16)0x2
#define TM_FREQ_1024 	(vu16)0x3

/* REG_RCNT bits */
#define RCNT_GPIO_DATA_MASK		0x000f
#define RCNT_GPIO_DIR_MASK		0x00f0
#define RCNT_GPIO_INT_ENABLE	(1 << 8)
#define RCNT_MODE_GPIO			0x8000

u8 *sram = (u8 *) 0x0E000000;

//song data structure
 
typedef struct {
	u8 volume; 		//0=off, 1=normal 2=accent
	s8 pitch;		//1024=normal pitch 
	u8 pan; 		//128= normal, 0=left, 255=right
} pattern_row; 

typedef struct {
	pattern_row rows[MAX_ROWS];
} pattern_column;  /* 256 bytes */

typedef struct pattern {
	pattern_column columns[MAX_COLUMNS];
} pattern;

#define MAX_PATTERNS	50
#define MAX_ORDERS		100

#define NO_ORDER		-1
 
#define HEADER_SIZE		150

typedef struct channel {
	u8 sample;
	u8 volume;
	u8 pitch;
	u8 pan;
	u8 status;
} channel;
 
typedef struct song {
	//Constant size (30 bytes)
	char tag[8];
	
	char song_name[8];
	float bpm;
	u8 shuffle;
	u8 loop_mode;
	u8 sync_mode;
	u8 color_mode;
	//u16 samples[MAX_ROWS];
	channel channels[MAX_ROWS];
	
	int pattern_length;
	int order_length;
	
	//char marker[10];
	
	//Dynamic size
	pattern patterns[MAX_PATTERNS];
	short order[MAX_ORDERS+1];
} song;
 
song *current_song;
pattern *current_pattern;
 
int current_pattern_index=0;
int playing_pattern_index;
int order_index;

pattern pattern_buff;

#define NOT_MUTED 	' '
#define MUTED 		'M'
#define SOLOED		'S'

//int channel_status[MAX_ROWS];

u8 order_buff;
int order_page = 0;

#define MAX_SAMPLES MSL_NSAMPS

char string_buff[50];

void set_bpm_to_millis(){
	millis_between_ticks = (int) (((1.0f/(current_song->bpm/60.0f))/0.0001f)/4)/6;
}

int set_shuffle(){
	/*
	shuffle	t/t	 %
	1   	1,1	 18	
	2   	2,10 17
	3   	3, 9 25	
	4   	4, 8 33
	5   	5, 7 42
	6   	6, 6 50
	7   	7, 5 58
	8   	8, 4 67
	9   	9, 3 75
	10 	   10, 2 83
	11	   11, 1 92
	*/
	
	ticks = current_song->shuffle;
	tocks = 12-current_song->shuffle;
	
	return  (int) 100.0f/(12.0/current_song->shuffle);
}


void setup_timers(){
	// Overflow every ~0.01 millisecond:
    // 0.0001/59.59e-9 = 1678.133915086424
    // 0x68e ticks @ FREQ_1 
	REG_TM2D= -0x68e;          
    REG_TM2C= TM_FREQ_1; 
	
    // cascade into tm3
    REG_TM3D= -millis_between_ticks;
    REG_TM3C= TM_ENABLE | TM_CASCADE | TM_IRQ;
}

void update_timers(){
    REG_TM3D= -millis_between_ticks;
    REG_TM3C=0;
    REG_TM3C= TM_ENABLE | TM_CASCADE | TM_IRQ;
}

void serial_interrupt(void) __attribute__ ((section(".iwram")));
void gpio_interrupt(void) __attribute__ ((section(".iwram")));	
void timer3_interrupt(void) __attribute__ ((section(".iwram")));
void vblank_interrupt(void) __attribute__ ((section(".iwram")));

void enable_serial_interrupt(){
	//change for serial comms	
	irqSet( IRQ_SERIAL, serial_interrupt );

	REG_RCNT = R_NORMAL;//RCNT_MODE_GPIO | RCNT_GPIO_INT_ENABLE;

	REG_SIOCNT = SIO_8BIT | SIO_IRQ;

	//set up interrupts for serial
	irqDisable(IRQ_TIMER3);
	irqEnable(IRQ_VBLANK | IRQ_SERIAL);
}

void disable_serial_interrupt(){
	//change for serial comms
	
	REG_RCNT = 0;
	irqDisable(IRQ_SERIAL);
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
}

void enable_trig_interrupt(){
	irqSet( IRQ_SERIAL, gpio_interrupt );
		
	REG_RCNT = RCNT_MODE_GPIO | RCNT_GPIO_INT_ENABLE;

	//set up interrupts for serial
	irqDisable(IRQ_TIMER3);
	irqEnable(IRQ_VBLANK | IRQ_SERIAL);
}

void disable_trig_interrupt(){
	REG_RCNT = 0;
	irqDisable(IRQ_SERIAL);
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
}

void init_song(){
	//Song Variables	
	memcpy(current_song->tag, "GBA DRUM", 8);
	memcpy(current_song->song_name, "SONGNAME", 8);
	
	current_song->bpm = 120;
	current_song->shuffle = 6; 	//6x6 = 50% shuffle
	set_bpm_to_millis();
	set_shuffle();
	
	current_song->loop_mode = SONG_MODE;
	current_song->color_mode = 0;
	
	//Pattern inits
	int pc;
	int row;
	int column;
	
	for (pc = 0; pc < MAX_PATTERNS; pc++){	
		for (column = 0; column < MAX_COLUMNS; column++){
			for (row = 0; row < MAX_ROWS; row++){
				pattern_row *current_cell = &(current_song->patterns[pc].columns[column].rows[row]);
				current_cell->volume = 0; 
				current_cell->pitch = 0;
				current_cell->pan = PAN_OFF;
			}
		}
	}

	//Orders inits
	int i;
	
	for (i = 0; i <= MAX_ORDERS; i++){
		current_song->order[i]=NO_ORDER;
	}
	
	//Samples inits
	for (i = 0; i < MAX_ROWS; i++){
		current_song->channels[i].sample = i;
		current_song->channels[i].volume = 200;
		current_song->channels[i].pitch = 12;
		current_song->channels[i].pan = PAN_CENTER;
		current_song->channels[i].status = NOT_MUTED;
	}	
}

void rand_pattern(){
	int row=0;
	int column=0;
	u8 val;
	
	pattern_row *current_cell = &(current_song->patterns[current_pattern_index].columns[column].rows[row]);
	for (column = 0; column < MAX_COLUMNS; column++){
		for (row = 0; row < MAX_ROWS; row++){			
			val = (u8)rand();
			if (val > 245) {	
				current_cell->volume = 2;
			} else if (val < 245 && val > 220){	
				current_cell->volume = 1;
			} else {
				current_cell->volume = 0;
			}	
				current_cell->pitch = 0;
				current_cell->pan = PAN_OFF;
				current_cell++;
			}
		}
}

void rand_samples(){
	int i, ii, val;
	
	for (i = 0; i < MAX_ROWS; i++){	
		val = rand() % MAX_SAMPLES;
		for (ii = 0; ii < MAX_ROWS; ii++){
			if (val == current_song->channels[ii].sample){
				val = rand() % MAX_SAMPLES;
				ii = 0;
			}
		}
		current_song->channels[i].sample = val;
	}
}

void clear_pattern(){
	int column, row;
	pattern_row *current_cell;
	
	for (column = 0; column < MAX_COLUMNS; column++){
		for (row = 0; row < MAX_ROWS; row++){
			current_cell = &(current_song->patterns[current_pattern_index].columns[column].rows[row]);
			current_cell->volume = 0; 
			current_cell->pitch = 0;
			current_cell->pan = PAN_OFF;
		}
	}

}

void copy_pattern(){
	pattern *from = &(current_song->patterns[current_pattern_index]);
	pattern *to = &pattern_buff;
	 
	memcpy(to, from, sizeof(pattern));
}		

void paste_pattern(){
	pattern *from = &pattern_buff;
	pattern *to = &(current_song->patterns[current_pattern_index]);
	 
	memcpy(to, from, sizeof(pattern));
}

void copy_order(short *from){
	if (*from != NO_ORDER){
		order_buff = *from;
	}
}

void paste_order(short *to){
	if (order_buff != NO_ORDER){
		*to = order_buff;
	}
}

void play_sound(int row, int sample, int vol, int pitch, int pan){
	if (active_sounds[row]){ 				//if theres a sound playing in that row
		mmEffectCancel(active_sounds[row]); //cancel it before trigging it again
	}
	
	//if (channel_status[row] != MUTED){		
	if (current_song->channels[row].status != MUTED){		
	//break out into function set_effect_params?
	mm_sound_effect sfx = {
	{sample} ,			// id
	(int)(1.0f * (pitch)),	// rate
	0,		// handle
	vol,	// volume
	pan,	// panning
	};
	
	active_sounds[row] = mmEffectEx(&sfx); //mmeffect returns mmhandler into the array
	//amb = mmEffectEx(&sfx); //mmeffect returns mmhandler into the array
	}
}
	
void play_column(void) __attribute__ ((section(".iwram")));

void play_column(){
	int row=0;
	int val,pan,pitch;
	pattern_row *pv;
		
	pv = &(current_song->patterns[playing_pattern_index].columns[current_column].rows[row]);
	
	
	for (row = 0; row < MAX_ROWS; row++){
		val = pv->volume;
		if (val != 0){
			if (pv->pan != PAN_OFF)
				pan = pv->pan;
			else
				pan = current_song->channels[row].pan;
			
			pitch = current_song->channels[row].pitch+pv->pitch;
			
			if (pitch < 0){
				pitch = 0;
			} else if (pitch > 24){
				pitch = 24;
			}
		}
			
		if (val == 1) {
			play_sound(row, 
			current_song->channels[row].sample, 
			current_song->channels[row].volume, 
			pitch_table[pitch], 
			pan);
		} else if (val == 2) {	
			play_sound(row, 
			current_song->channels[row].sample, 
			current_song->channels[row].volume+55, 
			pitch_table[pitch],
			pan);
		}
		pv++;
	}
}


void define_palettes(int palette_option){
	u32 i;
	u16 *temppointer;

	temppointer = BG_PALETTE;
	for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][0][i];		
	}
	temppointer = (BG_PALETTE+16);
	for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][1][i];		;
	}
	temppointer = (BG_PALETTE+32);
	for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][2][i];		;
	}
	temppointer = (BG_PALETTE+48);
		for(i=0; i<7; i++) {
		*temppointer++ = palettes[palette_option][3][i];		;
	}
}

void clear_screen(){
	// clear screen map with tile 0 ('space' tile) (256x256 halfwords)
	*((u32 *)MAP_BASE_ADR(31)) =0;
	CpuFastSet( MAP_BASE_ADR(31), MAP_BASE_ADR(31), FILL | COPY32 | (0x800/4));
}

void put_character(char c, u16 x, u16 y, unsigned char palette) {
	int offset;
	u16 value;

	if (c < 32)
		c = 32;
	value = ((u16) (c - 32)) | (((u16) palette) << 12);
	offset = (y * 32) + x;
	
	*((u16 *)MAPADDRESS + offset) = value;
}

void put_string(char *s, u16 x, u16 y, unsigned char palette) {
	u16 x_index = x;
	while (*s != 0 ) {
		if(*s == '\n'){
			s++;
			y++;
			x = x_index;
		}
		put_character(*s, x, y, palette);
		x++;
		s++;
	}
}

void set_palette(u16 x, u16 y, unsigned char palette) {
	int offset;
	u16 value;

	value = (((u16) palette) << 12);
	offset = (y * 32) + x;
	
	*((u16 *)MAPADDRESS + offset) &= 0x42;
	*((u16 *)MAPADDRESS + offset) |= value;
}

void draw_cursor(){
	if (current_screen == SCREEN_PATTERN) {
		if (draw_flag == 0){
			xmod = 5+(cursor_x/4);
			put_character('X', cursor_x+xmod, cursor_y+3, 1);
		}
	} else if (current_screen == SCREEN_SONG) {
		put_character('X', 2+(cursor_x)+(cursor_x*5), cursor_y+3, 1);
		
	} 
	else if (current_screen == SCREEN_SAMPLES) {
		if (draw_flag == 0){
			put_character('>', (cursor_x*6)+4, cursor_y+3, 1);
		}
	}
	else if (current_screen == SCREEN_SETTINGS) {
		if (draw_flag == 0){
			put_character('>', cursor_x, cursor_y+3, 1);
		}
	}
}

void draw_cell_status(){
	//26, 27, 28, 29
	pattern_row *pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
	
	if (pv->volume != 0){
		//pitch, pan, solo
		if (pv->pitch == 0){
			put_string("  ",26,3+cursor_y,0);
		} else if (pv->pitch > 0){
			sprintf(string_buff, "+%x", pv->pitch);
			put_string(string_buff,26,3+cursor_y,0);
		} else {
			sprintf(string_buff, "-%x", pv->pitch*-1);
			put_string(string_buff,26,3+cursor_y,0);
		}
		if (pv->pan == PAN_LEFT){
			put_character('L',28,3+cursor_y,0);
		} else if (pv->pan == PAN_RIGHT){
			put_character('R',28,3+cursor_y,0);
		} else if (pv->pan == PAN_CENTER){
			put_character(' ',28,3+cursor_y,0);
			}
	} else {
		put_string("   ",26,3+cursor_y,0);
	}
}

void move_cursor(int mod_x, int mod_y){
	int new_x, new_y;
	
	if (current_screen == SCREEN_PATTERN) {
		new_x = cursor_x + mod_x;
		new_y = cursor_y + mod_y;
		pattern_row *pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
		
		xmod = 5+(cursor_x/4);
		
		put_string("   ",26,3+cursor_y,0);
		
		if (pv->volume == 0){
			put_character('-', cursor_x+xmod,cursor_y+3, 0);
		} else if (pv->volume == 1){
			put_character('o', cursor_x+xmod,cursor_y+3, 0);
		} else {
			put_character('O', cursor_x+xmod,cursor_y+3, 0);
		}
		
		if (new_x >= MAX_COLUMNS) { 
			cursor_x = 0;
		} else if (new_x < 0) {
			cursor_x=MAX_COLUMNS-1;
		} else cursor_x=new_x;
		
		if (new_y >= MAX_ROWS) {
			cursor_y = 0;
		} else if (new_y < 0) {
			cursor_y=MAX_ROWS-1;
		} else cursor_y=new_y;		
		
		draw_cell_status();
		draw_cursor();
		
	} else if (current_screen == SCREEN_SONG) {
		new_x = cursor_x + mod_x;
		new_y = cursor_y + mod_y;
		
		if ((new_x < 5) && (new_x >= 0)){
			if ((new_y < 10) && (new_y >= 0)){
				put_character(':',2+(cursor_x)+(cursor_x*5),cursor_y+3,0);
				cursor_x = new_x;
				cursor_y = new_y;
				draw_cursor();
			}
		}
	} else if (current_screen == SCREEN_SAMPLES) {
		new_y = cursor_y + mod_y;
		new_x = cursor_x + mod_x;
		
		if ((new_x < 4) && (new_x >= 0)){
			if ((new_y < MAX_ROWS) && (new_y >= 0)){
				put_character(' ',(cursor_x*6)+4,cursor_y+3,0);
				cursor_x = new_x;
				cursor_y = new_y;
				draw_cursor();
			}
		}
	} else if (current_screen == SCREEN_SETTINGS) {
		new_y = cursor_y + mod_y;
		
		if ((new_y < 13) && (new_y >= 0)){
				put_character(' ',0,cursor_y+3,0);
				if (new_y == 3) new_y+=mod_y;
				if (new_y == 7) new_y+=mod_y;
				if (new_y == 11) new_y+=mod_y;
				cursor_y = new_y;
				draw_cursor();
			}
	} else if (current_screen == SCREEN_HELP) {
		if (mod_x != 0){
			new_x = cursor_x + mod_x;
			
			
			if ((new_x >= 0) && (new_x < 4)){
				cursor_y = 0;
				cursor_x = new_x;
				draw_help();
			}
		}
	} 
}

void draw_column_cursor(){
	if (draw_flag == 0){
		draw_flag = 1;
		xmod = 5+(current_column/4);
		put_character(' ',last_column+last_xmod,2,0);
		put_character('v',current_column+xmod,2,0);
		last_xmod = xmod;
		last_column = current_column;
		draw_flag = 0;
	}
}

void clear_last_song_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			put_character(' ',5+(((order_index-order_page-1)/10)*6),((order_index-order_page-1)%10)+3,0);
			draw_cursor();		
		}
	}
}

void clear_last_song_queued_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			put_character(' ',5+(((play_next)/10)*6),((play_next)%10)+3,0);
			draw_cursor();		
		}
	}
}

void draw_song_queued_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			put_character('-',5+(((play_next)/10)*6),((play_next)%10)+3,0);
			draw_cursor();		
		}
	}
}

void draw_song_cursor(){
	if (draw_flag == 0){
		if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
			put_character(' ',5+(((order_index-order_page-1)/10)*6),((order_index-order_page-1)%10)+3,0);
			draw_cursor();		
			put_character('<',5+(((order_index-order_page)/10)*6),((order_index-order_page)%10)+3,0);
		}
	}
}

void clear_song_cursor(){
	if ((order_index < 50 && order_page == 0) || (order_index >= 50 && order_page == 50)){
		put_character(' ',5+(((order_index-order_page)/10)*6),((order_index-order_page)%10)+3,0);
	}
}

void draw_status(){
		put_string(status_text[status],0,19,0);
}

void draw_temp_status(int temp_status){
		put_string(status_text[temp_status],0,19,0);
}

void set_status(int new_status){
	status = new_status;
	draw_status();
}
	
void draw_pattern(void) __attribute__ ((section(".iwram")));

void draw_pattern(){ 
	if (draw_flag == 0){
	draw_flag = 1;
	
	int r = 0;
	int c = 0;

	clear_screen();			
	sprintf(string_buff, "PATTERN %02d", current_pattern_index);
	put_string(string_buff,0,0,0);
	
	//make array, fill array with chars, make one printf
	char rowbuff[(MAX_ROWS+4)]; 
	int i;
	
	for (r = 0; r < MAX_ROWS; r++){
		i = 0;			
		for (c = 0; c < MAX_COLUMNS; c++){
			if ( c == 4 || c == 8 || c == 12) { //print extra space for 1/4 divide
				rowbuff[i] =' ';
				i++;
			}
					
			if (current_song->patterns[current_pattern_index].columns[c].rows[r].volume == 0){
				rowbuff[i] ='-';
			} else if (current_song->patterns[current_pattern_index].columns[c].rows[r].volume == 1) {
				rowbuff[i] ='o'; 
			} else {
				rowbuff[i] ='O';
			}
			i++; 
		}
		rowbuff[i] ='\0';
		sprintf(string_buff,"%02d [ %s ]   %c", r, rowbuff,current_song->channels[r].status);
		put_string(string_buff,0,r+3,0);
		}
		
	draw_status();
	draw_cell_status();
	draw_flag = 0;
	
	draw_cursor();
	
	}
}

void draw_song(){
	draw_flag = 1;
	
	clear_screen();			
	put_string("SONG",0,0,0);
	
	
	int column,row;
	int order_index,order;
	
	for (row = 0; row < 10; row++){
		for (column = 0; column < 5; column++){
			order_index=(column*10)+row+order_page;
			order = current_song->order[order_index];
			if (order == NO_ORDER){
				sprintf(string_buff, "%02d:-- ", order_index);
				put_string(string_buff,column*6,row+3,0);
			}else{	
				sprintf(string_buff, "%02d:%02d ", order_index,order);
				put_string(string_buff,column*6,row+3,0);
			}
		}
	}	
	draw_cursor();
	
	if (play_next != NO_ORDER){
		draw_song_queued_cursor();
	}
	
	draw_status();
	draw_flag = 0;
}

void draw_samples(){
	clear_screen();			
	put_string("SAMPLES",0,0,0);
	put_string("   [SMPL][VOL ][TUNE][ PAN ]",0,2,0);
	
	/*
	u8 sample;
	u8 volume;
	u8 pitch;
	u8 pan;
	u8 status; 
	*/
	
	int i;
	
	
	for (i = 0; i < MAX_ROWS; i++){	
		sprintf(string_buff, "%02d [ %02d ][ %02x ][ %02d ][ %s ]", 
		i, 
		current_song->channels[i].sample, 
		current_song->channels[i].volume, 
		current_song->channels[i].pitch, 
		pan_string[(current_song->channels[i].pan)/127]);		
		put_string(string_buff,0,i+3,0);
		//if (i == cursor_y) put_string(string_buff,0,i+3,2);
		//else put_string(string_buff,0,i+3,0);
	}
	
	draw_cursor();
	draw_status();
}

void draw_settings(){
	int row_index = 3;	

	clear_screen();			
	put_string("SETTINGS",0,0,0);
	
	sprintf(string_buff," SONG NAME: %s", current_song->song_name);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff," BPM: %.2d", (int) current_song->bpm);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff," SHUFFLE: %d%% - %d/%d", set_shuffle(), ticks, tocks);
	put_string(string_buff,0,row_index,0);
	row_index++;
	row_index++;

	sprintf(string_buff, " LOOP MODE: %s", loop_mode_text[current_song->loop_mode]);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff, " SYNC MODE: %s", sync_mode_text[current_song->sync_mode]);
	put_string(string_buff,0,row_index,0);
	row_index++;
	
	sprintf(string_buff, " COLOR MODE %d", current_song->color_mode);
	put_string(string_buff,0,row_index,0);
	row_index++;
	row_index++;
	
	put_string(" SAVE SONG\n LOAD SONG\n CLEAR SONG\n \n HELP",0,row_index,0);
	
	draw_cursor();
	draw_status();
}

void draw_help(){
	clear_screen();			
	put_string("HELP",0,0,0);
	put_string((char *) help_text[cursor_x],0,2,0);
}

void draw_screen(){
	cursor_x = 0;
	cursor_y = 0;
	
	switch (current_screen) {
	case SCREEN_PATTERN:
		draw_pattern();
		break;
	case SCREEN_SONG:
		draw_song();
		break;
	case SCREEN_SAMPLES:
		draw_samples();
		break;
	case SCREEN_SETTINGS:
		draw_settings();
		break;
	}
} 

int find_chain_start(index){
	int f;
	for (f = index -1; f > 0; f--){
		if (current_song->order[f-1] == NO_ORDER)
			return f;
	}  
	return 0;
}

void stop_song(){
	play = PLAY_STOPPED;
	if (current_screen == SCREEN_SONG){
		clear_song_cursor();
		if (play_next != NO_ORDER){
			clear_last_song_queued_cursor();
		}
	} else if (current_screen == SCREEN_PATTERN){
		put_character(' ',last_column+last_xmod,2,0);
	}		
	order_index = 0;
	current_column = 0;
	play_next=NO_ORDER;
			
	switch (current_song->sync_mode) {
		case SYNC_NONE:
			REG_TM2C &= !TM_ENABLE;
			set_status(STATUS_STOPPED);
			break;
		case SYNC_SLAVE:
			disable_serial_interrupt();	
			set_status(STATUS_SYNC_STOPPED);
			break;
		case SYNC_TRIG_SLAVE:
			disable_trig_interrupt();	
			set_status(STATUS_SYNC_STOPPED);
			break;
			
		}
		
}

void play_pattern(){
	current_column=0;
	playing_pattern_index=current_pattern_index;
	play = PLAY_PATTERN;
	
	switch (current_song->sync_mode) {
		case SYNC_NONE:		
			set_status(STATUS_PLAYING_PATTERN);
			REG_TM2C ^= TM_ENABLE;
			break;
		case SYNC_SLAVE:
			set_status(STATUS_SYNC_PATTERN);
			enable_serial_interrupt();
			break;
		case SYNC_MASTER:
			//pass
			break;
		case SYNC_TRIG_SLAVE:
			set_status(STATUS_SYNC_PATTERN);
			enable_trig_interrupt();
			break;
		case SYNC_TRIG_MASTER:
			//pass
			break;
		}
}

void play_song(){
	current_column=0;
	order_index = order_page+(cursor_x*10)+cursor_y;
	if (current_song->order[order_index]!=NO_ORDER){
		playing_pattern_index = current_song->order[order_page+(cursor_x*10)+cursor_y];
		if (current_song->loop_mode == SONG_MODE) {
			play = PLAY_SONG;
			
		} else if (current_song->loop_mode == LIVE_MODE) {
			play = PLAY_LIVE;		
		}
		
		switch (current_song->sync_mode) {
			case SYNC_NONE:
			case SYNC_MASTER:
			case SYNC_TRIG_MASTER:
				set_status(STATUS_PLAYING_SONG);
				REG_TM2C ^= TM_ENABLE;
				break;
			case SYNC_SLAVE:
				play_next = playing_pattern_index;
				draw_song_queued_cursor();
				set_status(STATUS_SYNC_SONG);
				enable_serial_interrupt();
				break;
			case SYNC_TRIG_SLAVE:
				play_next = playing_pattern_index;
				draw_song_queued_cursor();
				set_status(STATUS_SYNC_SONG);
				enable_trig_interrupt();
				break;
		}
		
	}
}

void next_column(){
	if (play == PLAY_PATTERN){  //pattern play mode
		if (current_column == MAX_COLUMNS){
			playing_pattern_index=current_pattern_index;
			current_column = 0;
		}
		play_column();
	
		if (current_screen == SCREEN_PATTERN){
			if (playing_pattern_index==current_pattern_index){ 
				if (draw_flag == 0){
					draw_column_cursor();
				}
			}
		}
	} else { //song or live play mode
	
		if (current_column == MAX_COLUMNS){
			if ((play == PLAY_LIVE) && (play_next!=NO_PATTERN)){
				if (current_screen == SCREEN_SONG){
					clear_song_cursor();
				}
				order_index = play_next;
				play_next = NO_PATTERN;
			} else {
				order_index++; // go to next order in array
			}
			playing_pattern_index=current_song->order[order_index];
			current_column = 0;
			if (current_screen == SCREEN_PATTERN){ //to remove the remaining cursor when playing patt changes
				
				put_character(' ',last_column+last_xmod,2,0);
			}	
		}
		
		if (current_screen == SCREEN_PATTERN){
			if (playing_pattern_index==current_pattern_index) draw_column_cursor();
		}
	
		if (playing_pattern_index == NO_ORDER){ //end of 'chain'
			if (play == PLAY_SONG ){
				if (current_screen == SCREEN_SONG){
					clear_last_song_cursor();
					clear_song_cursor();
				}
				stop_song();
			} else if (play == PLAY_LIVE){
				if (current_screen == SCREEN_SONG){
					clear_last_song_cursor();
					clear_song_cursor();
				} else if (current_screen == SCREEN_PATTERN){
					if (playing_pattern_index==current_pattern_index)
					put_character(' ',15,2,0); //clear pattern cursor
				}
				order_index = find_chain_start(order_index);
				playing_pattern_index=current_song->order[order_index];
				current_column = 0;	
				if (current_screen == SCREEN_SONG){
					clear_last_song_cursor();
					draw_song_cursor();
				}
				play_column();
			}
		} else {
			if (current_screen == SCREEN_SONG){
				draw_song_cursor();
			}
			play_column();
		}		
	}
current_column++;
}

int is_empty_pattern(int pattern_index){
	int c,r;
	
	for (r=0; r<MAX_ROWS; r++){
		for (c=0; c<MAX_COLUMNS; c++){
			if (current_song->patterns[pattern_index].columns[c].rows[r].volume != 0){
				return 0;
			}
		}
	}
	
	return 1;
}

void set_patterns_data(){
	/*Find how many patterns contain data, to discard empty patterns
	 * when saving data. Start at zero, count back until data, 
	 */
	int i;
	
	for (i = MAX_PATTERNS-1; i >= 0; i--){
		if (!is_empty_pattern(i)){
			current_song->pattern_length = i+1;
			//sprintf(string_buff, "%d", current_song->pattern_length);
			//put_string(string_buff,0,0,0);
			break;
		}
	
	}  
}

void set_orders_data(){
	int i;
	
	for (i = MAX_ORDERS; i >= 0; i--){
		if (current_song->order[i]!=NO_ORDER){
			break;
		}
	current_song->order_length = i;
	}
}

void file_option_save(void) {
	u16 *from;
	u8 *to;
	u32 i;
	
	set_patterns_data();
	set_orders_data();	
	
	/*double size = sizeof(song) - (sizeof(pattern) * MAX_PATTERNS) - (sizeof(u8)*(MAX_ORDERS+1));
	
	sprintf(string_buff, "len2 %d , %d, %d", (int) size, current_song->pattern_length, current_song->order_length);
			put_string(string_buff,0,1,0);
	 */
	from = (u16 *) current_song;
	to = (u8 *) sram;
	//Song vars
	
	for (i = 0; i < 195; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
		
		//memcmp(i, &current_song->patterns)
		if (from == (u16 *) current_song->patterns){	
			//sprintf(string_buff, "len2 %d %d", (int)i, current_song->order_length);
			//put_string(string_buff,0,1,0);
			put_character('b',0,19,0);
			break;
		}
	
	}
	//Pattern data
	from = (u16 *) current_song->patterns;
	int plen = current_song->pattern_length * sizeof(pattern);
	for (i=0; i < plen; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
	}
	//Order data
	from = (u16 *) current_song->order;
	for (i=0; i < current_song->order_length; i++) {
		*to = (u8) (*from & 0x00FF);
		to++;
		*to = (u8) (*from >> 8);
		to++;
		from++;
	}

	//int saved = (int) (from - ((u16 *) current_song));
	//iprintf("\x1b[19;0Hsaved %d", saved);	//debug
}

void file_option_load(void) {
	u32 i;
	u16 *to;
	u8 *from;
	
	from = (u8 *) sram;
	to = (u16 *) current_song;

	//Song vars
	for (i=0; i < 195; i++) {
		u16 acc = *from;
		from++;
		acc |= ((u16) (*from)) << 8;
		*to = acc;
		to++;
		from++;
		if (to == (u16 *) current_song->patterns){
			//sprintf(string_buff, "len %d %d", (int)i, (int)from);
			//put_string(string_buff,0,1,0);
			break;
		}
	}
	
	//iprintf("\x1b[17;0Hplen %d", current_song->pattern_length);	//debug
	//iprintf("\x1b[18;0Holen %d", current_song->order_length);	//debug
	
	//Pattern data
	to = (u16 *) current_song->patterns;
	int pmax = sizeof(current_song->patterns);
	int plen = current_song->pattern_length * sizeof(pattern); 
	for (i=0; i < pmax; i++) {
		if (i < plen){
			u16 acc = *from;
			from++;
			acc |= ((u16) (*from)) << 8;
			*to = acc;
			to++;
			from++;
		} else {
			*to = 0;
			to++;
		}
	}
	//Order data
	to = (u16 *) current_song->order;
	for (i=0; i < MAX_ORDERS+1; i++) {
		if (i < current_song->order_length){
			u16 acc = *from;
			from++;
			acc |= ((u16) (*from)) << 8;
			*to = acc;
			to++;
			from++;
		} else {
			*to = NO_ORDER;
			to++;
	}
	}
	
	set_bpm_to_millis();
	set_shuffle();
	update_timers();
	draw_screen();
	
	/*
	int loaded = (int) (to - ((u16 *) current_song));
	iprintf("\x1b[19;0Hloaded %d / %d", size,loaded);	//debug
	*/
}

void samples_menu(int channel_index, int channel_item, int action){
	int preview_sound = 0;
	
	if (action == MENU_ACTION){
		preview_sound = 1;	
	} else if (action == MENU_LEFT){
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				if (current_song->channels[channel_index].sample > 0){
					current_song->channels[channel_index].sample--;
					draw_samples();					
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				if (current_song->channels[channel_index].volume > 0){
					current_song->channels[channel_index].volume--;
					draw_samples();					
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch > 0){
					current_song->channels[channel_index].pitch--;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PAN:
				if (current_song->channels[channel_index].pan == PAN_CENTER){
					current_song->channels[channel_index].pan = PAN_LEFT;
					draw_samples();
				} else if (current_song->channels[channel_index].pan == PAN_RIGHT){
					current_song->channels[channel_index].pan = PAN_CENTER;
					draw_samples();
				}
				break;
		}
	} else if (action == MENU_RIGHT){
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				//inc sample by 1}
				if (current_song->channels[channel_index].sample < MAX_SAMPLES){
					current_song->channels[channel_index].sample++;
					draw_samples();
					
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				if (current_song->channels[channel_index].volume < 200){
					current_song->channels[channel_index].volume++;
					draw_samples();					
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch < 24){
					current_song->channels[channel_index].pitch++;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PAN:
				if (current_song->channels[channel_index].pan == PAN_CENTER){
					current_song->channels[channel_index].pan = PAN_RIGHT;
					draw_samples();
				} else if (current_song->channels[channel_index].pan == PAN_LEFT) {
					current_song->channels[channel_index].pan = PAN_CENTER;
					draw_samples();
				}
				break;
		}
	} else if (action == MENU_UP){
		int item_mod;
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				//inc sample by 10}
				item_mod = current_song->channels[channel_index].sample + 10;
				if (item_mod <= MAX_SAMPLES){
					current_song->channels[channel_index].sample = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].sample = MAX_SAMPLES;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				item_mod = current_song->channels[channel_index].volume + 10;
				if (item_mod <= 200){
					current_song->channels[channel_index].volume = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].volume = 200;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch < 14){
					current_song->channels[channel_index].pitch+=10;					
				} else {
					current_song->channels[channel_index].pitch=24;					
				}
				draw_samples();
				preview_sound = 1;
				break;
			case CHANNEL_PAN:
				//do stuff
				break;
		}
	} else if (action == MENU_DOWN){
		int item_mod;
		switch (channel_item) {
			case CHANNEL_SAMPLE:
				//dec sample by 10
				item_mod = current_song->channels[channel_index].sample - 10;
				if (item_mod > 0){
					current_song->channels[channel_index].sample = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].sample = 0;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_VOLUME:
				item_mod = current_song->channels[channel_index].volume - 10;
				if (item_mod > 0){
					current_song->channels[channel_index].volume = item_mod;
					draw_samples();
				} else {
					current_song->channels[channel_index].volume = 0;
					draw_samples();
				}
				preview_sound = 1;
				break;
			case CHANNEL_PITCH:
				if (current_song->channels[channel_index].pitch > 10){
					current_song->channels[channel_index].pitch-=10;
					
				} else {
					current_song->channels[channel_index].pitch=0;
				}
				draw_samples();
				preview_sound = 1;

				break;
			case CHANNEL_PAN:
				//do stuff
				break;
		}
	}
	if (preview_sound == 1){
		play_sound(channel_index, 
		current_song->channels[channel_index].sample, 
		current_song->channels[channel_index].volume, 
		pitch_table[current_song->channels[channel_index].pitch],
		current_song->channels[channel_index].pan );
	}
		
}

int name_index;

void settings_menu(int menu_option, int action){
	if (menu_option == 0) {
		//song name
		if (action == MENU_ACTION){
			if (strcmp(current_song->song_name,"SONGNAME")==0){
				memcpy(current_song->song_name,"        ",8);
				draw_settings();
			}
			put_character(current_song->song_name[name_index],12+name_index, 3, 1);
		} else {
		if (action == MENU_LEFT){
			if (name_index > 0){
				put_character(current_song->song_name[name_index],12+name_index, 3, 0);
				name_index--;
				put_character(current_song->song_name[name_index],12+name_index, 3, 1);
			}
		} else if (action == MENU_RIGHT){
			if (name_index < 7){
				put_character(current_song->song_name[name_index],12+name_index, 3, 0);
				name_index++;
				put_character(current_song->song_name[name_index],12+name_index, 3, 1);				
			}
		} else if (action == MENU_UP){
			if ((current_song->song_name[name_index] < 'Z') && (current_song->song_name[name_index] >= 'A')){
				current_song->song_name[name_index]++;
			} else if (current_song->song_name[name_index] < 'A'){
				current_song->song_name[name_index]='A';
			}
				//draw_settings();
				put_character(current_song->song_name[name_index],12+name_index, 3, 1);
			
		} else if (action == MENU_DOWN){
			if ((current_song->song_name[name_index] <= 'Z') && (current_song->song_name[name_index] > 'A')){
				current_song->song_name[name_index]--;
			} else if (current_song->song_name[name_index] < 'A'){
				current_song->song_name[name_index]='A';
			}
				//draw_settings();
				put_character(current_song->song_name[name_index],12+name_index, 3, 1);
			
		}
		} 
	} else if (menu_option == 1) { 	
		//bpm

		if (action == MENU_LEFT){
			if (current_song->bpm > MIN_BPM){
				current_song->bpm--;
			}
		} else if (action == MENU_RIGHT){
			if (current_song->bpm < MAX_BPM){
				current_song->bpm++;
			}
		} else if (action == MENU_UP){
			current_song->bpm+=10;
			if (current_song->bpm > MAX_BPM){
				current_song->bpm=MAX_BPM;
			}
		} else if (action == MENU_DOWN){
			current_song->bpm-=10;
			if (current_song->bpm < MIN_BPM){
				current_song->bpm=MIN_BPM;
			}
		}
		set_bpm_to_millis();
		update_timers();
		draw_settings();
	} else if (menu_option == 2) {
		//shuffle
		if (action == MENU_LEFT){
			if (current_song->shuffle > 1){
				current_song->shuffle--;
				set_shuffle();
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->shuffle < 11){
				current_song->shuffle++;
				set_shuffle();
				draw_settings();
			}
		}
	} else if (menu_option == 4) {
		//loop mode
		if (action == MENU_LEFT){
			if (current_song->loop_mode == LIVE_MODE){
				current_song->loop_mode = SONG_MODE;
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->loop_mode == SONG_MODE){
				current_song->loop_mode = LIVE_MODE;
				draw_settings();
			}
		}
	} else if (menu_option == 5) {
		//sync mode
		if (action == MENU_LEFT){			
			if (current_song->sync_mode > 0){
				stop_song();
				current_song->sync_mode--;
				stop_song();
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->sync_mode < 5){
				stop_song();
				current_song->sync_mode++;				
				stop_song();
				draw_settings();
			}
		}
	} else if (menu_option == 6) {
		//color mode
		if (action == MENU_LEFT){			
			if (current_song->color_mode > 0){
				current_song->color_mode--;
				define_palettes(current_song->color_mode);	
				draw_settings();
			}
		} else if (action == MENU_RIGHT){
			if (current_song->color_mode < 2){
				current_song->color_mode++;
				define_palettes(current_song->color_mode);
				draw_settings();
			}
		}
	} else if (menu_option == 8) {
		if (action == MENU_ACTION){
			stop_song();
			file_option_save();
			draw_settings();
			draw_temp_status(STATUS_SAVE);
		}
	} else if (menu_option == 9) {
		if (action == MENU_ACTION){
			stop_song();
			file_option_load();
			draw_settings();
			draw_temp_status(STATUS_LOAD);
		}
	} else if (menu_option == 10) {
		if (action == MENU_ACTION){
			init_song();
			draw_settings();
			draw_temp_status(STATUS_CLEAR);
		}
	} else if (menu_option == 12) {
		if (action == MENU_ACTION){
			current_screen = SCREEN_HELP;
			draw_help();
		}
	}
}

void process_input(){
	int keys_pressed, keys_held, index;
	
	scanKeys();

	keys_pressed = keysDown();
	//keys_released = keysUp();
	keys_held = keysHeld();
		
	//PATTERN SCREEN INPUTS
	if (current_screen == SCREEN_PATTERN){	
		pattern_row *pv;

		if ( keys_pressed & KEY_B ) {			
			if (keys_held & KEY_SELECT ) {
				copy_pattern();
				rand_pattern();
				draw_pattern();
			}
		}

		if ( keys_pressed & KEY_A ) {			
			if (keys_held & KEY_B ) { 			//if A is already held, delete cell
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
				pv->volume = 0;
				pv->pitch = 0;
				//pv->pan = 0;
				draw_cell_status();
			} else if (keys_held & KEY_SELECT ) {
				copy_pattern();
				clear_pattern();
				draw_pattern();
			}
		}
		
		if ( keys_pressed & KEY_LEFT ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			if (keys_held & KEY_A ) {       //depitch by 1 semitone
				if(pv->volume > 0){
					int mod_pitch = (pv->pitch-1) + current_song->channels[cursor_y].pitch;
					if (mod_pitch>0){
						pv->pitch--;
					} 
					draw_cell_status();					
				} 
				} else if (keys_held & KEY_B ) {
					if(pv->volume > 0){
						xmod = 5+(cursor_x/4);
						if(pv->pan == PAN_RIGHT){
							pv->pan = PAN_CENTER;
						} else {
							pv->pan = PAN_LEFT;
						}
						draw_cell_status();
					}			
				} else {		
				move_cursor(-1,0);
			}
		}
		
		if ( keys_pressed & KEY_RIGHT ) {		
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			if (keys_held & KEY_A ) {
				if(pv->volume > 0){
					int mod_pitch = (pv->pitch+1) + current_song->channels[cursor_y].pitch;
					if (mod_pitch < 24){
						pv->pitch++;
					} 
						draw_cell_status();
				}
			} else if (keys_held & KEY_B ) {
				if(pv->volume > 0){
					if (pv->pan == PAN_LEFT){
						pv->pan=PAN_CENTER;
					} else {	
						pv->pan=PAN_RIGHT;
					}
					draw_cell_status();
				} 
			} else {		
				move_cursor(1,0);
			}			
		}
		
		if ( keys_pressed & KEY_UP ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);
			if (keys_held & KEY_A ) {
				if (pv->volume == 0){
					pv->pan = PAN_OFF;
				} 
				if (pv->volume < 2){
					pv->volume++;
				}
				draw_cell_status();
			} else if (keys_held & KEY_B ) {
				//up octave
			} else if (keys_held & KEY_SELECT ) {
				//MUTE/UNMUTE CHANNEL
				if (current_song->channels[cursor_y].status != MUTED){
					current_song->channels[cursor_y].status = MUTED;
					put_character('M',29,cursor_y+ymod,0);
				} else {
					current_song->channels[cursor_y].status = NOT_MUTED;
					put_character(' ',29,cursor_y+ymod,0);			
				}
			} else if (keys_held & KEY_START ) {
				//DO NOTHING
			} else {
				move_cursor(0,-1);
			}
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			pv = &(current_song->patterns[current_pattern_index].columns[cursor_x].rows[cursor_y]);			
			if (keys_held & KEY_B ) {
				//down octave
			} else if (keys_held & KEY_A ) {
				if (pv->volume > 0){
					pv->volume--;
				}
				draw_cell_status();
			} else if (keys_held & KEY_SELECT ) {
				int i;
				if (current_song->channels[cursor_y].status != SOLOED){
				//SOLO/UNSOLO CHANNEL
					for (i = 0; i < MAX_ROWS; i++){					
						if (current_song->channels[i].status != SOLOED){
							current_song->channels[i].status = MUTED;
							put_character('M',29,i+ymod,0);
						} else {
							put_character('S',29,i+ymod,0);
						}
					}
					current_song->channels[cursor_y].status = SOLOED;
					put_character('S',29,cursor_y+ymod,0);
				} else {
					for (i = 0; i < MAX_ROWS; i++){					
						current_song->channels[i].status = NOT_MUTED;
						put_character(' ',29,i+ymod,0);
					}
				}			
			} else if (keys_held & KEY_START ) {
				//DO NOTHING
			} else {
				move_cursor(0,1);
			}
		}
		
		if ( (keys_pressed & KEY_START) && ((keys_held ^ KEY_START) == 0)) {
			if (play == PLAY_STOPPED){ 
				play_pattern();
			}
			else if (play == PLAY_LIVE){ //play this column next
				if ( keys_held & KEY_SELECT ) {
					stop_song();
				}
			} else {
				stop_song();
			}
		}
					
		if ( keys_pressed & KEY_R ) {
			if (keys_held & KEY_SELECT ) {
				current_screen = SCREEN_SONG;
				draw_screen();					
			}
			else if (keys_held & KEY_A){
				paste_pattern();
				draw_pattern();
			} else {
				if (current_pattern_index < (MAX_PATTERNS-1)) {
					current_pattern_index+=1;
				}
				draw_pattern();
			}			
		}

		if ( keys_pressed & KEY_L ) {
			if (keys_held & KEY_A){
				copy_pattern();
			} else if (current_pattern_index > 0) {
				current_pattern_index-=1;				
			}
			draw_pattern();
		}
	}
	
	//SONG SCREEN INPUTS
	else if (current_screen == SCREEN_SONG){
		if ( keys_pressed & KEY_LEFT ) {
			if (keys_held & KEY_A ) {
				if (current_song->order[order_page+(cursor_x*10)+cursor_y] > 0){
					current_song->order[order_page+(cursor_x*10)+cursor_y]-=1;
					draw_song();
				}
			} else {
				move_cursor(-1,0);
		}
	}
		if ( keys_pressed & KEY_RIGHT ) {
			if (keys_held & KEY_A ) {
					if (current_song->order[order_page+(cursor_x*10)+cursor_y] < MAX_PATTERNS-1){
						current_song->order[order_page+(cursor_x*10)+cursor_y]+=1;
						draw_song();
				}
			} else {
				move_cursor(1,0);
			}
		}
		
		if ( keys_pressed & KEY_UP ) {
			if (keys_held & KEY_A ) {
				index = order_page+cursor_y+(cursor_x*10);
				if (current_song->order[index] < MAX_PATTERNS-11){
					current_song->order[index]+=10;
				} else {
					current_song->order[index] = MAX_PATTERNS-1;	
				}
				draw_song();
			} else {
			move_cursor(0,-1);
			}
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			if (keys_held & KEY_A ) {
				index = order_page+cursor_y+(cursor_x*10);
				if (current_song->order[index] > 10){
					current_song->order[index]-=10;
				} else {
					current_song->order[index] = 0;
				}
				draw_song();				
			} else {
			move_cursor(0,1);
			}
		}
	
	if ( keys_pressed & KEY_A ) {			
			if (keys_held & KEY_SELECT ) {
					paste_order(&current_song->order[order_page+(cursor_x*10)+cursor_y]);
					move_cursor(0,1);
					draw_song();
				} else if (keys_held & KEY_B ) {
					copy_order(&current_song->order[order_page+(cursor_x*10)+cursor_y]);
					current_song->order[order_page+(cursor_x*10)+cursor_y]=-1;
					draw_song();
				} else if (keys_held == KEY_A ){
					if (current_song->order[order_page+(cursor_x*10)+cursor_y] == NO_ORDER){
						current_song->order[order_page+(cursor_x*10)+cursor_y] = 0;
						draw_song();
					}
				}  
			}
	
	if ( keys_pressed & KEY_L ) {
		if ( keys_held & KEY_SELECT ) {
			current_screen = SCREEN_PATTERN;
			draw_screen();
		} else {
			order_page=0;
			draw_song();
		}
	}
	
	if ( keys_pressed & KEY_R ) {
		if ( keys_held & KEY_SELECT ) {
			current_screen = SCREEN_SAMPLES;
			draw_screen();
		} else {
			order_page=50;
			draw_song();
		}
	}
			
	if ( keys_pressed & KEY_START ) {
		if (play == PLAY_STOPPED){ //start playing
			play_song();
		} else if (play == PLAY_LIVE){ //if livemode, queue next pattern
			if ( keys_held & KEY_SELECT ) {
				stop_song();
			} else {
				if (current_song->order[order_page+(cursor_x*10)+cursor_y] != NO_ORDER){
					if (play_next != NO_ORDER){
						clear_last_song_queued_cursor();
					}
					play_next = order_page+(cursor_x*10)+cursor_y;
					draw_song_queued_cursor();
				}
			}
		} else { 			//stop playing
			stop_song();	
			}
		}
	} 
	
	//SAMPLE SCREEN INPUTS
	else if (current_screen == SCREEN_SAMPLES){
		if ( keys_pressed & KEY_LEFT ) {
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x, MENU_LEFT);
			} else if ((keys_held ^ KEY_LEFT) == 0){
				move_cursor(-1,0);	
			}
		}
		
		if ( keys_pressed & KEY_RIGHT ) {
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x,  MENU_RIGHT);
			} else if ((keys_held ^ KEY_RIGHT) == 0){
				move_cursor(1,0);	
			}	
		}
		
		if ( keys_pressed & KEY_UP ) { 
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x,  MENU_UP);
			} else if ((keys_held ^ KEY_UP) == 0){
				move_cursor(0,-1);
			}	
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			if (keys_held & KEY_A) {
				samples_menu(cursor_y, cursor_x,  MENU_DOWN);
			} else if ((keys_held ^ KEY_DOWN) == 0){
				move_cursor(0,1);	
			}
		}
		
		if ( keys_pressed & KEY_A ) {
			if ((keys_held ^ KEY_A) == 0){
				samples_menu(cursor_y, cursor_x, MENU_ACTION);
			}
		}
		
		if ( keys_pressed & KEY_B ) {
			if ( keys_held & KEY_SELECT ) {
				if (cursor_x == 0){
					rand_samples();
					draw_samples();
				}
			}
		}

		if ( keys_pressed & KEY_L ) {
			if ( keys_held & KEY_SELECT ) {
				current_screen = SCREEN_SONG;
				draw_screen();
			}
		}
			
		if ( keys_pressed & KEY_R ) {
			if ( keys_held & KEY_SELECT ) {
				current_screen = SCREEN_SETTINGS;
				draw_screen();
			}
		}
	}	
	
	//SETTINGS SCREEN INPUTS
	else if (current_screen == SCREEN_SETTINGS){
		//process input
		if ( keys_pressed & KEY_LEFT ) {
			if ( keys_held & KEY_SELECT ) {
				//sync time adjust
			} else if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_LEFT);
			}
		}
		
		if ( keys_pressed & KEY_RIGHT ) {
			if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_RIGHT);
			}	
		}
		
		if ( keys_pressed & KEY_UP ) { 
			if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_UP);
			} else if ((keys_held ^ KEY_UP) == 0){
				move_cursor(0,-1);
			}	
		}
		
		if ( keys_pressed & KEY_DOWN ) {
			if (keys_held & KEY_A) {
				settings_menu(cursor_y, MENU_DOWN);
			} else if ((keys_held ^ KEY_DOWN) == 0){
				move_cursor(0,1);	
			}
		}
		
		if ( keys_pressed & KEY_A ) {
			if ((keys_held ^ KEY_A) == 0){
				settings_menu(cursor_y, MENU_ACTION);
			}
		}
		
		if ( keys_pressed & KEY_L ) {
			if ( keys_held & KEY_SELECT ) {
				current_screen = SCREEN_SAMPLES;
				draw_screen();
			}
		}
	} 
	
	//HELP SCREEN INPUTS
	else if (current_screen == SCREEN_HELP){
		if (keys_pressed & KEY_LEFT){
			move_cursor(-1,0);
		}
		if (keys_pressed & KEY_RIGHT){
			move_cursor(1,0);
		}
		
		if ( keys_pressed & KEY_A || keys_pressed & KEY_B) {
			current_screen = SCREEN_SETTINGS;
			draw_screen();
		}
	}
}

void vblank_interrupt(){
		mmVBlank();
		REG_IF = IRQ_VBLANK;
	}


void timer3_interrupt(){
	    current_ticks++;
	    
	    if (current_song->sync_mode == SYNC_MASTER){
			//send a serial byte i guess?
		} else if (current_song->sync_mode == SYNC_TRIG_MASTER){
			//pull so high?
		}
	    
	    if (tick_or_tock == TICK){		
			if (current_ticks >= ticks){				
				tick_or_tock = TOCK;	
				current_ticks = 0;
				next_column();
				
			}
		} else if (tick_or_tock == TOCK){
			if (current_ticks >= tocks){     //if its a tock
				tick_or_tock = TICK;
				current_ticks = 0;
				next_column();
			}
		}
		//REG_IF = REG_IF | IRQ_TIMER3;
		REG_IF = IRQ_TIMER3;
}	

			
void gpio_interrupt(){
	/*
	 * 24ppqn, so each pulse is a tick 24pulse = 1/4, 6 pulse = 1/16
	 * 12ppqn, so each pulse is 2           12 = 1/4, 3 = 1/16 
	*/
	    current_ticks+=2;
	    if (tick_or_tock == TICK){       
			if (current_ticks >= ticks){
				tick_or_tock = TOCK;	
				current_ticks = 0;
				next_column();
				}
			} else if (current_ticks >= tocks){     //if its a tock
				tick_or_tock = TICK;
				current_ticks = 0;
				next_column();
				}
	
	REG_IF = IRQ_SERIAL; //apparently to 'reset' or acknowledge the interrupt?
}

void serial_interrupt(){
	/* clear the data register */
	REG_SIODATA8 = 0;

	/* XXX p.134 says to set d03 of SIOCNT ... */
	/* IE=1, external clock */
	REG_SIOCNT = SIO_IRQ | SIO_SO_HIGH;

	/* XXX then it says to set it 0 again... I don't get it */
	REG_SIOCNT &= ~SIO_SO_HIGH;
	
	/* start transfer */
	REG_SIOCNT |= SIO_START;
	
	current_ticks++; 	//1 tick per byte
    
    if (tick_or_tock == TICK){       
		if (current_ticks >= ticks){
			tick_or_tock = TOCK;	
			current_ticks = 0;
			next_column();
			}
		} else if (current_ticks >= tocks){     //if its a tock
			tick_or_tock = TICK;
			current_ticks = 0;
			next_column();
		}

	//serial_ticks++;
	REG_IF = IRQ_SERIAL; //apparently to 'reset' the interrupt?
}

void setup_interrupts(){
	irqInit();
	irqSet( IRQ_VBLANK, vblank_interrupt );
	irqSet( IRQ_TIMER3, timer3_interrupt );
	//irqSet( IRQ_SERIAL, gpio_interrupt );
	irqEnable(IRQ_VBLANK | IRQ_TIMER3);
	mmSetVBlankHandler( &mmFrame );
}


void setup_display(){
	// screen mode & background to display
	SetMode( MODE_0 | BG0_ON );
	// set the screen base to 31 (0x600F800) and char base to 0 (0x6000000)
	BGCTRL[0] = SCREEN_BASE(31);
	// load the palette for the background, 7 colors
	define_palettes(current_song->color_mode);
	// load the font into gba video mem (48 characters, 4bit tiles)   
	CpuFastSet(r6502_portfont_bin, (u16*)VRAM,(r6502_portfont_bin_size/4) | COPY32);
	// clear screen map with tile 0 ('space' tile) (256x256 halfwords)
	*((u32 *)MAP_BASE_ADR(31)) =0;
	CpuFastSet( MAP_BASE_ADR(31), MAP_BASE_ADR(31), FILL | COPY32 | (0x800/4));
}


int main() {
	current_song = (song *) malloc(sizeof(song));
	init_song();
	
	setup_timers();
	
	setup_interrupts();

	setup_display();

	// initialise maxmod with soundbank and 8 channels
    mmInitDefault( (mm_addr)soundbank_bin, MAX_ROWS );

	//GLOBAL VARS
	cursor_x = 0;
	cursor_y = 0;
	current_screen = SCREEN_PATTERN;
	current_ticks = 0;
	
	draw_screen();
	
	while (1) {
		VBlankIntrWait();
		process_input();
	}
}


