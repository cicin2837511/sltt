#define _XOPEN_SOURCE 500
#include <SDL3/SDL.h>
#include <fontconfig/fontconfig.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef VERSION
#define VERSION "unknown"
#endif
#define NAME "sltt"
#define TITLE NAME " " VERSION

typedef struct {
    char c;
} cell;

struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool running;
    TTF_Font *font;
    TTF_Text *text;
    TTF_TextEngine *engine;
    int w, h;
    int textw, texth;
    int rows, cols;
    cell **grid;
    pid_t pty_pid;
    int master;
    char ptyoutput[4097];
} term = {
    .window = NULL,
    .renderer = NULL,
    .running = true,
    .grid = NULL
};
 
void die(const char *s);
void *xmalloc(size_t size);
void sdlcheck(int code, const char *s);
void *sdlcheckptr(void *ptr, const char *s);
char *fcfonttopath(void);
void initialize(void);
void quit(void);
void handleinput(void);
void update(void);
void updategrid(const char *s);
void render(void);

void
die(const char *s) {
    perror(s);
    exit(1);
}

void *
xmalloc(size_t size) {
    void *ptr;
    ptr = malloc(size);
    if (ptr == NULL) die("malloc");
    return ptr;
}

void
sdlcheck(int code, const char *s) {
    if (!code) {
        printf("%s: %s", s, SDL_GetError());
        exit(1);
    }
}

void *
sdlcheckptr(void *ptr, const char *s) {
    if (ptr == NULL) {
        printf("%s: %s", s, SDL_GetError());
        quit();
        exit(1);
    }
    return ptr;
}

void
initialize(void) {
    sdlcheck(SDL_Init(SDL_INIT_VIDEO), "SDL_Init");
    sdlcheck(TTF_Init(), "TTF_Init");
    term.window = sdlcheckptr(SDL_CreateWindow(TITLE, 800, 600, SDL_WINDOW_RESIZABLE), "SDL_CreateWindow");
    sdlcheck(SDL_StartTextInput(term.window), "SDL_StartTextInput");
    term.renderer = sdlcheckptr(SDL_CreateRenderer(term.window, NULL), "SDL_CreateRenderer");

    term.font = sdlcheckptr(TTF_OpenFont(fcfonttopath(), 30), "TTF_OpenFont");
    term.engine = sdlcheckptr(TTF_CreateRendererTextEngine(term.renderer), "TTF_CreateRendererTextEngine");
    term.text = sdlcheckptr(TTF_CreateText(term.engine, term.font, " ", 1), "TTF_CreateText");
}

char *
fcfonttopath(void) {
    FcInit();
    FcPattern *pattern = FcNameParse((const FcChar8 *)"DepartureMonoNerdFont");
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern *font = FcFontMatch(NULL, pattern, &result);

    char *ret;
    if (font) {
        FcChar8 *file = NULL;
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
            ret = strdup((const char*) file);
        }
        FcPatternDestroy(font);
    }
    FcPatternDestroy(pattern);
    FcFini();
    return ret;
}

void
quit(void) {
    SDL_DestroyWindow(term.window);
    SDL_DestroyRenderer(term.renderer);
    SDL_Quit();
}

void
handleinput(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) term.running = false;
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            write(term.master, e.text.text, 1);
        }
    }
}

void
update(void) {
    SDL_GetWindowSize(term.window, &term.w, &term.h);
    TTF_GetTextSize(term.text, &term.textw, &term.texth);
    term.rows = term.h/term.texth;
    term.cols = term.w/term.textw;
    static size_t len = 0;
    ssize_t n;
    while (len < sizeof(term.ptyoutput) - 1 &&
            (n = read(term.master, term.ptyoutput + len,
                      sizeof(term.ptyoutput) - 1 - len)) > 0) {
        len += n;
    }
    term.ptyoutput[len] = 0;
    updategrid(term.ptyoutput);
}

void
updategrid(const char *s) {
    term.grid = xmalloc(sizeof(cell*) * term.rows);
    for (int i = 0; i < term.rows; i++) {
        term.grid[i] = xmalloc(sizeof(cell) * term.cols);
    }
    for (int i = 0; i < term.rows; i++) {
        for (int j = 0; j < term.cols; j++) {
            term.grid[i][j].c = ' ';
        }
    }
    int idx = (memcmp(term.ptyoutput, "\x1b[?2004h", 8) == 0) ? 8 : 0;
    for (int i = 0; i < term.rows; i++) {
        for (int j = 0; j < term.cols; j++) {
            if (s[idx] == '\0') break;
            if (s[idx] == '\n') {
                idx++;
                break;
            }
            term.grid[i][j].c = s[idx++];
        }
    }
}

void
render(void) {
    SDL_SetRenderDrawColor(term.renderer, 0, 0, 0, 255);
    SDL_RenderClear(term.renderer);

    TTF_SetTextColor(term.text, 255, 255, 255, 255);
    for (int i = 0; i < term.rows; i++) {
        for (int j = 0; j < term.cols; j++) {
            const char *cstr = &term.grid[i][j].c;
            TTF_SetTextString(term.text, cstr, 1);
            TTF_DrawRendererText(term.text, j*term.textw, i*term.texth);
        }
    }

    SDL_RenderPresent(term.renderer);
}

int
main(void) {
    initialize();
    term.pty_pid = forkpty(&term.master, 0, 0, 0);
    if (term.pty_pid < 0) die("forkpty");
    fcntl(term.master, F_SETFL, O_NONBLOCK);
    if (term.pty_pid == 0) {
        execlp("sh", "sh", NULL);
        die("execl");
    }
    while (term.running) {
        handleinput();
        update();
        render();
    }
    quit();
    return 0;
}
