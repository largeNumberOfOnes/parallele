#include <stdlib.h>
#include <unistd.h>

#include "../../slibs/err_proc.h"



typedef char cell;
const cell CELL_DEAD  = 0;
const cell CELL_ALIVE = 1;
const cell CELL_DYING = 2;
const cell CELL_NEWBORN = 3;

typedef struct Message_t {
    int skip;
    int buffer_size;
    cell *buffer;
} Message;

typedef struct Index_t {

} Index;

Index index_init() {
    return (Index) {};
}

typedef struct Time_t {

} Time;

Time time_init() {
    return (Time) {};
};

// ------------------------------------------------------------------- Game

typedef struct Game_t {
    int w;
    int h;
    cell *grid;
    Index index;
    void (*start_time)    (Time *time, const Index *index);
    void (*exchange_edge) (Message *message, const Index *index);
    void (*print)         (const struct Game_t *game, const Index *index);
    void (*end_time)      (Time *time, const Index *index);
    void (*exchenge_time) (Time *time, const Index *index);
} Game;

Game game_init(
    int w,
    int h,
    Index index,
    void (*start_time)    (Time *time, const Index *index),
    void (*exchange_edge) (Message *message, const Index *index),
    void (*print)         (const struct Game_t *game, const Index *index),
    void (*end_time)      (Time *time, const Index *index),
    void (*exchenge_time) (Time *time, const Index *index)
) {
    cell *grid = (cell*) malloc(sizeof(cell) * w * h);
    srand(7);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // grid[y * w + x] = (rand() % 20 == 7) ? CELL_ALIVE : CELL_DEAD;
            grid[y * w + x] = CELL_DEAD;
        }
    }
    for (int x = 0; x < 3; ++x) {
            grid[0 * w + x + 3] = CELL_ALIVE;
        }
    return (Game) {
        .w = w,
        .h = h,
        .grid = grid,
        .index = index,
        .start_time          = start_time,
        .exchange_edge       = exchange_edge,
        .print               = print,
        .end_time            = end_time,
        .exchenge_time       = exchenge_time
    };
}

void game_dent(Game *game) {
    free(game->grid);
}

cell *game_get_cell(Game *game, int x, int y) {
    return &game->grid[y * game->w + x];
}

cell game_get_cell_const(const Game *game, int x, int y) {
    return game->grid[y * game->w + x];
}

int game_is_alive(const Game *game, int x, int y) {
    return game_get_cell_const(game, x, y) == CELL_ALIVE
        || game_get_cell_const(game, x, y) == CELL_DYING;
}

int game_is_dead(Game *game, int x, int y) {
    return game_get_cell_const(game, x, y) == CELL_DEAD
        || game_get_cell_const(game, x, y) == CELL_NEWBORN;
}

void game_make_alive(Game *game, int x, int y) {
    *game_get_cell(game, x, y) = CELL_ALIVE;
}
void game_make_newborn(Game *game, int x, int y) {
    *game_get_cell(game, x, y) = CELL_NEWBORN;
}
void game_make_dead(Game *game, int x, int y) {
    *game_get_cell(game, x, y) = CELL_DEAD;
}
void game_make_dying(Game *game, int x, int y) {
    *game_get_cell(game, x, y) = CELL_DYING;
}

static int count_neighbors(Game *game, int x, int y) {
    int count = 0;
    for (int dy = -1; dy < 2; ++dy) {
        for (int dx = -1 ; dx < 2; ++dx) {
            if (!(dx == 0 && dy == 0)) {
                count += game_is_alive(
                    game,
                    x + dx,
                    (y + dy >= 0) ? ((y + dy) % game->h) : (game->h - 1)
                );
            }
        }
    }
    return count;
}

static void print_neightbors_count(Game *game) {
    for (int y = 0; y < game->h; ++y) {
        for (int x = 1; x + 1 < game->w; ++x) {
            int count = count_neighbors(game, x, y);
            printf("%d", count);
        }
        printf("\n");
    }
}

static void evalute(Game *game) {
    for (int y = 0; y < game->h; ++y) {
        for (int x = 1; x + 1 < game->w; ++x) {
            int count = count_neighbors(game, x, y);
            if (game_is_alive(game, x, y)) {
                if (count < 2 || 3 < count) {
                    game_make_dying(game, x, y);
                }
            } else {
                if (count == 3) {
                    game_make_newborn(game, x, y);
                }
            }
        }
    }
    for (int y = 0; y < game->h; ++y) {
        for (int x = 1; x + 1 < game->w; ++x) {
            cell *cell_ptr = game_get_cell(game, x, y);
            if (*cell_ptr == CELL_DYING) {
                *cell_ptr = CELL_DEAD;
            }
            if (*cell_ptr == CELL_NEWBORN) {
                *cell_ptr = CELL_ALIVE;
            }
        }
    }
}

void game_start_game_loop(Game *game) {
    Time time = time_init();
    while (1) {
        game->print(game, &game->index);
        // game->start_time(&time);
        // game->exchange_long_edge(game);
        // game->exchange_short_edge(game);
        evalute(game);
        // game->end_time(&time);
        // game->exchenge_time(&time);
        DOT
        sleep(1);
    }
}

// ------------------------------------------------------------------------

void start_time(Time *time, const Index *index) {}
void exchange_edge (Message *message, const Index *index) {}
void print(const struct Game_t *game, const Index *index) {
    for (int y = 0; y < game->h; ++y) {
        for (int x = 0; x < game->w; ++x) {
            if (game_is_alive(game, x, y)) {
                printf("#");
            } else {
                printf("_");
            }
        }
        printf("\n");
    }
}
void end_time(Time *time, const Index *index) {}
void exchenge_time(Time *time, const Index *index) {}

// ------------------------------------------------------------------------


int main() {

    Index index = index_init();
    Game game = game_init(
        10,
        10,
        index,
        start_time,
        exchange_edge,
        print,
        end_time,
        exchenge_time
    );

    game_start_game_loop(&game);


    return 0;
}
