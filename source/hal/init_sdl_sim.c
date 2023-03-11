//
// Created by Samuel Jones on 2/21/22.
//

#include "init.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <SDL.h>
#include "framebuffer.h"
#include "display_s6b33.h"
#include "led_pwm.h"
#include "button.h"
#include "button_sdl_ui.h"
#include "ir.h"
#include "rtc.h"
#include "flash_storage.h"
#include "led_pwm_sdl.h"
#include "sim_lcd_params.h"

#define UNUSED __attribute__((unused))

static int sim_argc;
static char** sim_argv;

// Forward declaration
void hal_start_sdl(int *argc, char ***argv);

// Do hardware-specific initialization.
void hal_init(void) {

    S6B33_init_gpio();
    led_pwm_init_gpio();
    button_init_gpio();
    ir_init();
    S6B33_reset();
    rtc_init_badge(0);
}

void *main_in_thread(void* params) {
    int (*main_func)(int, char**) = params;
    main_func(sim_argc, sim_argv);
    return NULL;
}

int hal_run_main(int (*main_func)(int, char**), int argc, char** argv) {

    sim_argc = argc;
    sim_argv = argv;

    pthread_t app_thread;
    pthread_create(&app_thread, NULL, main_in_thread, main_func);

    // Should not return until GTK exits.
    hal_start_sdl(&argc, &argv);

    return 0;
}

void hal_deinit(void) {
    flash_deinit();
    printf("stub fn: %s in %s\n", __FUNCTION__, __FILE__);
}

void hal_reboot(void) {
    printf("stub fn: %s in %s\n", __FUNCTION__, __FILE__);
    exit(0);
}

uint32_t hal_disable_interrupts(void) {
    printf("stub fn: %s in %s\n", __FUNCTION__, __FILE__);
    return 0;
}

void hal_restore_interrupts(__attribute__((unused)) uint32_t state) {
    printf("stub fn: %s in %s\n", __FUNCTION__, __FILE__);
}

// static GtkWidget *vbox, *drawing_area;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *pix_buf;
static char *program_title;
extern int lcd_brightness;

static void draw_led_text(SDL_Renderer *renderer, int x, int y)
{
#define LETTER_SPACING 12
    /* Literally draws L E D */
    /* Draw L */
    SDL_RenderDrawLine(renderer, x, y, x, y - 10);
    SDL_RenderDrawLine(renderer, x, y, x + 8, y);

    x += LETTER_SPACING;

    /* Draw E */
    SDL_RenderDrawLine(renderer, x, y, x, y - 10);
    SDL_RenderDrawLine(renderer, x, y, x + 8, y);
    SDL_RenderDrawLine(renderer, x, y - 5, x + 5, y - 5);
    SDL_RenderDrawLine(renderer, x, y - 10, x + 8, y - 10);

    x += LETTER_SPACING;

    /* Draw D */
    SDL_RenderDrawLine(renderer, x, y, x, y - 10);
    SDL_RenderDrawLine(renderer, x, y, x + 8, y);
    SDL_RenderDrawLine(renderer, x, y - 10, x + 8, y - 10);
    SDL_RenderDrawLine(renderer, x + 8, y - 10, x + 10, y - 5);
    SDL_RenderDrawLine(renderer, x + 8, y, x + 10, y - 5);
}

void flareled(unsigned char r, unsigned char g, unsigned char b)
{
    led_color.red = r;
    led_color.green = g;
    led_color.blue = b;
}


static int draw_window(SDL_Renderer *renderer, SDL_Texture *texture)
{
    extern uint8_t display_array[LCD_YSIZE][LCD_XSIZE][3];


    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Draw the pixels of the screen */


    /* This:
     *
     *    SDL_UpdateTexture(texture, NULL, display_array, LCD_XSIZE * 3);
     *
     * doesn't work right for some reason I don't quite.  The texture apparently
     * wants the data in BGRA order.
     *
     * In any case, for now, we can copy display_array inserting the
     * alpha channel that SDL_RenderCopy seems to expect. Modern
     * computers can do this copy in microseconds, so it's not a big deal.
     */

    /* Copy display_array[] but add on an alpha channel. SDL_RenderCopy() seems to need it.
     * Plus we try to use it to implement LCD brightness. */
    static uint8_t display_array_with_alpha[LCD_YSIZE][LCD_XSIZE][4];

    float level = (float) lcd_brightness / 255.0f;
    for (int y = 0; y < LCD_YSIZE; y++) {
        for (int x = 0; x < LCD_XSIZE; x++) {
            /* SDL texture seems to want data in BGRA order, and since we're copying
             * anyway, we can emulate LCD brightness here too. */
            display_array_with_alpha[y][x][2] = (uint8_t) (level * display_array[y][x][0]);
            display_array_with_alpha[y][x][1] = (uint8_t) (level * display_array[y][x][1]);
            display_array_with_alpha[y][x][0] = (uint8_t) (level * display_array[y][x][2]);
	    /* I tried to implement lcd brightness via alpha channel, but it doesn't seem to work */
            /* display_array_with_alpha[y][x][3] = 255 - lcd_brightness; */
            display_array_with_alpha[y][x][3] = 255;
        }
    }
    SDL_UpdateTexture(texture, NULL, display_array_with_alpha, LCD_XSIZE * 4);
    struct sim_lcd_params slp = get_sim_lcd_params();
    SDL_Rect from_rect = { 0, 0, LCD_XSIZE, LCD_YSIZE };
    SDL_Rect to_rect = { slp.xoffset, slp.yoffset, slp.width, slp.height };
    SDL_RenderCopy(renderer, texture, &from_rect, &to_rect);

    int x, y;

    /* Draw a border around the simulated screen */
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderDrawLine(renderer, slp.xoffset - 1, slp.yoffset - 1., slp.xoffset + slp.width + 1, slp.yoffset - 1); /* top */
    SDL_RenderDrawLine(renderer, slp.xoffset - 1, slp.yoffset - 1., slp.xoffset - 1, slp.yoffset + slp.height + 1); /* left */
    SDL_RenderDrawLine(renderer, slp.xoffset - 1, slp.yoffset + slp.height + 1, slp.xoffset + slp.width + 1, slp.yoffset + slp.height + 1); /* bottom */
    SDL_RenderDrawLine(renderer, slp.xoffset + slp.width + 1, slp.yoffset - 1, slp.xoffset + slp.width + 1, slp.yoffset + slp.height + 1); /* right */

    /* Draw simulated flare LED */
    x = slp.xoffset + slp.width + 20;
    y = slp.yoffset + (slp.height / 2) - 20;
    draw_led_text(renderer, x, y);
    SDL_SetRenderDrawColor(renderer, led_color.red, led_color.blue, led_color.green, 0xff);
    SDL_RenderFillRect(renderer, &(SDL_Rect) { x, y + 20, 51, 51} );
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderDrawRect(renderer, &(SDL_Rect) { x, y + 20, 50, 50} );

    SDL_RenderPresent(renderer);
    return 0;
}

static void enable_sdl_fullscreen_sanity(void)
{
	/* If SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS isn't set to zero,
	 * fullscreen window behavior is *insane* by default.
	 *
	 * Alt-tab and Alt-left-arrow and Alt-right-arrow will *minimize*
	 * the window, pushing it to the bottom of the stack, so when you
	 * alt-tab again, and expect the window to re-appear, it doesn't.
	 * Instead, a different window appears, and you have to alt-tab a
	 * zillion times through all your windows until you finally get to
	 * the bottom where your minimized fullscreen window sits, idiotically.
	 *
	 * Let's make sanity the default.  The last parameter of setenv()
	 * says do not overwrite the value if it is already set. This will
	 * allow for any completely insane individuals who somehow prefer
	 * this idiotc behavior to still have it.  But they will not get
	 * it by default.
	 */

	char *v = getenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS");
	if (v && strncmp(v, "1", 1) == 0) {
		fprintf(stderr, "You have SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS set to 1!\n");
		fprintf(stderr, "I highly recommend you set it to zero. But it's your sanity\n");
		fprintf(stderr, "at stake, not mine, so whatever. Let's proceed anyway.\n");
	}
	setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0", 0);	/* Final 0 means don't override user's prefs */
								/* I am Very tempted to set it to 1. */
}

static int start_sdl(void)
{
    enable_sdl_fullscreen_sanity();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Unable to initialize SDL (Video):  %s\n", SDL_GetError());
        return 1;
    }
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "Unable to initialize SDL (Events):  %s\n", SDL_GetError());
        return 1;
    }
    atexit(SDL_Quit);
    return 0;
}

static void setup_window_and_renderer(SDL_Window **window, SDL_Renderer **renderer, SDL_Texture **texture)
{
    char window_title[1024];

    snprintf(window_title, sizeof(window_title), "HackRVA Badge Emulator - %s", program_title);
    free(program_title);
    *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               0, 0, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (!*window) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        exit(1);
    }
    // SDL_SetWindowSize(*window, SIM_SCREEN_WIDTH, SIM_SCREEN_HEIGHT);
    SDL_SetWindowFullscreen(*window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    *renderer = SDL_CreateRenderer(*window, -1, 0);
    if (!*renderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        exit(1);
    }

    *texture = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC, LCD_XSIZE, LCD_YSIZE);
    if (!*texture) { 
        fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);
    SDL_ShowWindow(*window);
    SDL_RenderClear(*renderer);
    SDL_RenderPresent(*renderer);
}

static void process_events(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            key_press_cb(&event.key.keysym);
            break;
        case SDL_KEYUP:
            key_release_cb(&event.key.keysym);
            break;
        case SDL_QUIT:
            /* Handle quit requests (like Ctrl-c). */
            time_to_quit = 1;
            break;
        case SDL_WINDOWEVENT:
            break;
        case SDL_MOUSEBUTTONDOWN:
            break;
        case SDL_MOUSEBUTTONUP:
            break;
        case SDL_MOUSEMOTION:
            break;
        case SDL_MOUSEWHEEL:
            break;
        }
    }
}

static void wait_until_next_frame(void)
{
    static uint32_t next_frame = 0;

    if (next_frame == 0)
        next_frame = SDL_GetTicks() + 33;  /* 30 Hz */
    uint32_t now = SDL_GetTicks();
    if (now < next_frame)
        SDL_Delay(next_frame - now);
    next_frame += 33; /* 30 Hz */
}

void hal_start_sdl(UNUSED int *argc, UNUSED char ***argv)
{
    int first_time = 1;

    program_title = strdup((*argv)[0]);
    if (start_sdl())
	exit(1);
    setup_window_and_renderer(&window, &renderer, &pix_buf);
    flareled(0, 0, 0);

    while (!time_to_quit) {
	draw_window(renderer, pix_buf);

	if (first_time) {
            int sx, sy;
            SDL_GetWindowSize(window, &sx, &sy);
            adjust_sim_lcd_params_defaults(sx, sy);
            set_sim_lcd_params_default();
            first_time = 0;
        }

	process_events();
	wait_until_next_frame();
    }

    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    printf("\n\n\n\n\n\n\n\n\n\n\n");
    printf("If you seak leak sanitizer complaining about memory and _XlcDefaultMapModifiers\n");
    printf("it's because SDL is programmed by monkeys.\n");
    printf("\n\n\n\n\n\n\n\n\n\n\n");
    exit(0);
}
