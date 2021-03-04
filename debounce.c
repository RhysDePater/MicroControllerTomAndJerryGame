/*
**	NonblockingDebounceDemo.c
**
**	A non-blocking poll-based de-bounce in tactile switch.
**	(Adapted from unattributed source code used in CAB202
**	Semester 1, 2017)
**
**	Lawrence Buckingham, QUT, September 2017.
**	(C) Queensland University of Technology.
*/
#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>

#include <graphics.h>
#include <macros.h>
#include "lcd_model.h"

typedef enum
{
    false,
    true
} bool;

void draw_all(void);
void draw_int(uint8_t x, uint8_t y, int value, colour_t colour);
bool left_button_clicked(void);

void setup(void)
{
    set_clock_speed(CPU_8MHz);
    lcd_init(LCD_DEFAULT_CONTRAST);
    draw_all();
    CLEAR_BIT(DDRD, 1); // up
    CLEAR_BIT(DDRF, 6); // left_button
}

// State machine for "button pressed"
bool pressed = false;
uint16_t closed_num = 0;
uint16_t open_num = 0;

#define THRESHOLD (1000)

bool left_button_clicked(void)
{
    // Detect a Click on left button
    bool was_pressed = pressed;

    if (BIT_IS_SET(PINF, 6))
    {
        closed_num++;
        open_num = 0;

        if (closed_num > THRESHOLD)
        {
            if (!pressed)
            {
                closed_num = 0;
            }

            pressed = true;
        }
    }
    else
    {
        open_num++;
        closed_num = 0;

        if (open_num > THRESHOLD)
        {
            if (pressed)
            {
                open_num = 0;
            }

            pressed = false;
        }
    }

    return was_pressed && !pressed;
}

// Auxiliary variables
uint16_t counter = 0;
char buffer[10];

void process(void)
{
    if (left_button_clicked())
    {
        counter++;
        draw_all();
    }
    // Display and wait if joystick up.
    if (BIT_IS_SET(PIND, 1))
    {
        draw_all();

        while (BIT_IS_SET(PIND, 1))
        {
            // Block until joystick released.
        }
    }
}

int main(void)
{
    setup();

    for (;;)
    {
        process();
    }
}
// -------------------------------------------------
// Helper functions.
// -------------------------------------------------
void draw_all(void)
{
    clear_screen();
    draw_string(0, 0, "DelayDebounceDemo", FG_COLOUR);
    draw_string(0, 10, "Clk lft to count", FG_COLOUR);
    draw_string(0, 20, "Joystk up to view", FG_COLOUR);
    draw_int(10, 30, counter, FG_COLOUR);
    show_screen();
}

void draw_int(uint8_t x, uint8_t y, int value, colour_t colour)
{
    snprintf(buffer, sizeof(buffer), "%d", value);
    draw_string(x, y, buffer, colour);
}