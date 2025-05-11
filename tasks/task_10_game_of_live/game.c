/**
 * Game of Life
 * Here we present three realizations of the game: a sequential version,
 *     a version for a distributed memory system based on the MPI
 *     interface, and a hybrid version running on two nodes with
 *     different memory pools and containing the same number of threads
 *     communicating via shared memory. Each version can be run in two
 *     modes: fast with a large playing field and without frame-by-frame
 *     rendering, and with field rendering. The mode is defined by the
 *     GMODE macro, which takes values 0 and 1. Version is defined by
 *     GTYPE macro, which accepts values from one to three in the
 *     specified order.
 * Macro GCOUNT determines count of executers for second version and
 *     count of threads for third version of game.
 * If macro USE_SKIP_OPTIMIZATION does not defined MPI version will not
 *     use skip optimization.
 */

#ifndef GTYPE
    #define GTYPE 0
#endif
#ifndef GMODE
    #define GMODE 0
#endif
#ifndef GCOUNT
    #define GCOUNT 1
#endif

#define USE_SKIP_OPTIMIZATION

#if GTYPE < 0 || 2 < GTYPE
    #error "GTYPE must be in range 0, 2"
#endif
#if GMODE < 0 || 1 < GMODE
    #error "GMODE must be in range 0, 1"
#endif
#if GCOUNT < 0
    #error "GCOUNT must be positive"
#endif

#include <assert.h>
#include <bits/pthreadtypes.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "../../slibs/err_proc.h"
#include "mpi_proto.h"



const int TAG_EDGE = 1;

typedef char cell;
const cell CELL_DEAD    = 0;
const cell CELL_ALIVE   = 1;
const cell CELL_DYING   = 2;
const cell CELL_NEWBORN = 3;

typedef struct Index_t {
    int node;
    int node_count;
    int rank;
    int rank_count;
} Index;

Index index_init(int node, int node_count, int rank, int rank_count) {
    return (Index) {
        .node = node,
        .node_count = node_count,
        .rank = rank,
        .rank_count = rank_count,
    };
}

struct Game_t;

typedef struct Message_t {
    int skip_left;
    int skip_right;
    int buffer_size;
    cell *left_far;
    cell *left_near;
    cell *right_far;
    cell *right_near;
    struct Game_t *game;
} Message;

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
    int skip_right;
    int skip_left;
    void (*start_time)    (Time *time, void *adat, const Index *index);
    void (*exchange_edge) (Message *message, void *adat, const Index *index);
    void (*print)         (const struct Game_t *game, const Index *index);
    void (*end_time)      (Time *time, void *adat, const Index *index);
    void (*exchange_time) (Time *time, void *adat, const Index *index);
    void *adat; // Additinal data for different realizations
} Game;

void draw_glider(cell *grid, int w, int h, int x, int y) {
    #define SET(x, y) grid[(x) * h + (y)] = CELL_ALIVE;

    SET(x + 0, y + 1);
    SET(x + 1, y + 2);
    SET(x + 2, y + 0);
    SET(x + 2, y + 1);
    SET(x + 2, y + 2);

    #undef SET
}

void draw_lwss(cell *grid, int w, int h) {
    #define SET(x, y) grid[x * h + y] = CELL_ALIVE;

    SET(2, 2);
    SET(2, 4);
    SET(3, 5);
    SET(4, 5);
    SET(5, 2);
    SET(5, 5);
    SET(6, 3);
    SET(6, 4);
    SET(6, 5);

    #undef SET
}

// If you use this init function, you should free grid before calling
//    game_dstr() and seintot NULL for game.grid field.
Game game_init_with_grid(
    int w,
    int h,
    Index index,
    cell *grid,
    void (*start_time)    (Time *time, void *adat, const Index *index),
    void (*exchange_edge) (Message *message, void *adat, const Index *index),
    void (*print)         (const struct Game_t *game, const Index *index),
    void (*end_time)      (Time *time, void *adat, const Index *index),
    void (*exchenge_time) (Time *time, void *adat, const Index *index),
    void *adat
) {
    return (Game) {
        .w = w,
        .h = h,
        .grid = grid,
        .index = index,
        .skip_right = 0,
        .skip_left  = 0,
        .start_time          = start_time,
        .exchange_edge       = exchange_edge,
        .print               = print,
        .end_time            = end_time,
        .exchange_time       = exchenge_time,
        .adat                = adat
    };
}

Game game_init(
    int w,
    int h,
    Index index,
    void (*start_time)    (Time *time, void *adat, const Index *index),
    void (*exchange_edge) (Message *message, void *adat, const Index *index),
    void (*print)         (const struct Game_t *game, const Index *index),
    void (*end_time)      (Time *time, void *adat, const Index *index),
    void (*exchenge_time) (Time *time, void *adat, const Index *index),
    void *adat
) {
    cell *grid = (cell*) malloc(sizeof(cell) * w * h);
    srand(7);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            // grid[y * w + x] = (rand() % 20 == 7) ? CELL_ALIVE : CELL_DEAD;
            grid[x * h + y] = CELL_DEAD;
        }
    }
    if (index.rank == 0) {
        draw_lwss(grid, w, h);
        // draw_glider(grid, w, h, 2, 2);
    }
    return (Game) {
        .w = w,
        .h = h,
        .grid = grid,
        .index = index,
        .skip_right = 0,
        .skip_left  = 0,
        .start_time          = start_time,
        .exchange_edge       = exchange_edge,
        .print               = print,
        .end_time            = end_time,
        .exchange_time       = exchenge_time,
        .adat                = adat
    };
}

void game_dstr(Game *game) {
    free(game->grid);
}

cell *game_get_cell(Game *game, int x, int y) {
    return &game->grid[x * game->h + y];
}

cell game_get_cell_const(const Game *game, int x, int y) {
    return game->grid[x * game->h + y];
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
    for (int dx = -1 ; dx < 2; ++dx) {
        for (int dy = -1; dy < 2; ++dy) {
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
    int skip_left  = 0;
    int skip_right = game->w - 2;
    int is_skip_left_blocked = 0;
    for (int x = 1; x + 1 < game->w; ++x) {
        int is_all_dead = 1;
        for (int y = 0; y < game->h; ++y) {
            cell *cell_ptr = game_get_cell(game, x, y);
            if (*cell_ptr == CELL_DYING) {
                *cell_ptr = CELL_DEAD;
            }
            if (*cell_ptr == CELL_NEWBORN) {
                *cell_ptr = CELL_ALIVE;
            }
            if (*cell_ptr == CELL_ALIVE) {
                is_all_dead = 0;
            }
        }
        if (is_all_dead) {
            if (!is_skip_left_blocked) {
                ++skip_left;
            }
        } else {
            is_skip_left_blocked = 1;
            skip_right = game->w - x - 2;
        }
    }
    #ifdef USE_SKIP_OPTIMIZATION
        game->skip_left  = skip_left;
        game->skip_right = skip_right;
    #else
        game->skip_left  = 0;
        game->skip_right = 0;
    #endif
}

void game_start_game_loop(Game *game) {
    Time time = time_init();
    #if GMODE == 1
        game->print(game, &game->index);
    #endif
    while (1) {
        #if GMODE == 0
            game->start_time(&time, game->adat, &game->index);
        #endif
        evalute(game);
        int w = game->w;
        int h = game->h;
        Message message = {
            .skip_left   = game->skip_left,
            .skip_right  = game->skip_right,
            .left_far    = game->grid,
            .left_near   = game->grid + h,
            .right_far   = game->grid + h * (w - 1),
            .right_near  = game->grid + h * (w - 2),
            .buffer_size = game->h,
            .game        = game
        };
        game->exchange_edge(&message, game->adat, &game->index);
        // usleep(100000);
        #if GMODE == 1
            game->print(game, &game->index);
            usleep(50000);
        #endif
        #if GMODE == 0
            game->end_time(&time, game->adat, &game->index);
            game->exchange_time(&time, game->adat, &game->index);
        #endif
    }
}

// ---------------------------------------------------- Default realization

static clock_t start, end;
const static int clock_timer_start = 100;
static int clock_timer = clock_timer_start;
void start_time(Time *time, void *adat, const Index *index) {
    if (index->rank == 0 && index->node == 0
                                    && clock_timer == clock_timer_start) {
        start = clock();
    }
}
void exchange_edge (Message *message, void *adat, const Index *index) {}
void print(const struct Game_t *game, const Index *index) {}
void end_time(Time *time, void *adat, const Index *index) {
    if (index->rank == 0 && index->node == 0) {
        if ( !clock_timer) {
            end = clock();
            printf("time: %f\n", (double)(end - start)/CLOCKS_PER_SEC);
            fflush(stdout);
            clock_timer = clock_timer_start;
        } else {
            --clock_timer;
        }
    }
}
void exchenge_time(Time *time, void *adat, const Index *index) {}

// ----------------------------------------------------- Sequamtial version

void print_sequential(const struct Game_t *game, const Index *index) {
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

// ------------------------------------------------------------ MPI version

typedef struct Comm_t {
    int recv_left;
    int send_left;
    int recv_right;
    int send_right;
} Comm;

static void comm_timer_step(Comm *comm) {
    if (comm->recv_left  > 0) { --comm->recv_left;  }
    if (comm->send_left  > 0) { --comm->send_left;  }
    if (comm->recv_right > 0) { --comm->recv_right; }
    if (comm->send_right > 0) { --comm->send_right; }

    if (comm->recv_left == 0) { comm->send_left = 0; }
    if (comm->send_left == 0) { comm->recv_left = 0; }

    if (comm->recv_right == 0) { comm->send_right = 0; }
    if (comm->send_right == 0) { comm->recv_right = 0; }
}

static void comm_set_new_val(Comm *comm, Message *message, int skips[2]) {
    #define MAX(x, y) ((x) > (y)) ? (x) : (y)

    if (comm->recv_left  == 0) {
        comm->recv_left  = MAX(skips[0] - 1, 0);
    }
    if (comm->send_left  == 0) {
        comm->send_left  = MAX(message->skip_left - 1, 0);
    }
    if (comm->recv_right == 0) {
        comm->recv_right = MAX(skips[1] - 1, 0);
    }
    if (comm->send_right == 0) {
        comm->send_right = MAX(message->skip_right - 1, 0);
    }

    #undef MAX
}

static void first_stage(
    Message *message,
    cell *temp,
    const Comm *comm,
    int rank,
    int size
) {
    cell *right_far  = message->right_far;
    cell *right_near = message->right_near;
    cell *left_far   = message->left_far;
    cell *left_near  = message->left_near;
    int count = message->buffer_size;
    if (rank % 2 == 0 && rank + 1 != size) {
        if (!comm->send_right) {
            RET_IF_ERR(
                MPI_Send(
                    right_near, count, MPI_CHAR,
                    rank + 1, TAG_EDGE, MPI_COMM_WORLD
                )
            );
        }
        if (!comm->recv_right) {
            RET_IF_ERR(
                MPI_Recv(
                    temp, count, MPI_CHAR, 
                    rank + 1, TAG_EDGE, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                )
            );
            memcpy(right_far, temp, count*sizeof(cell));
        }
    } else if (rank % 2 == 1) {
        if (!comm->recv_left) {
            RET_IF_ERR(
                MPI_Recv(
                    temp, count, MPI_CHAR, 
                    rank - 1, TAG_EDGE, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                )
            );
            memcpy(left_far, temp, count*sizeof(cell));
        }
        if (!comm->send_left) {
            RET_IF_ERR(
                MPI_Send(
                    left_near, count, MPI_CHAR,
                    rank - 1, TAG_EDGE, MPI_COMM_WORLD
                )
            );
        }
    }
}

static void second_stage(
    Message *message,
    cell *temp,
    const Comm *comm,
    int rank,
    int size
) {
    cell *right_far  = message->right_far;
    cell *right_near = message->right_near;
    cell *left_far   = message->left_far;
    cell *left_near  = message->left_near;
    int count = message->buffer_size;
    if (rank % 2 == 0 && rank != 0) {
        if (!comm->send_left) {
            RET_IF_ERR(
                MPI_Send(
                    left_near, count, MPI_CHAR,
                    rank - 1, TAG_EDGE, MPI_COMM_WORLD
                )
            );
        }
        if (!comm->recv_left) {
            RET_IF_ERR(
                MPI_Recv(
                    temp, count, MPI_CHAR, 
                    rank - 1, TAG_EDGE, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                )
            );
            memcpy(left_far, temp, count*sizeof(cell));
        }
    } else if (rank % 2 == 1 && rank + 1 != size) {
        if (!comm->recv_right) {
            RET_IF_ERR(
                MPI_Recv(
                    temp, count, MPI_CHAR, 
                    rank + 1, TAG_EDGE, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                )
            );
            memcpy(right_far, temp, count*sizeof(cell));
        }
        if (!comm->send_right) {
            RET_IF_ERR(
                MPI_Send(
                    right_near, count, MPI_CHAR,
                    rank + 1, TAG_EDGE, MPI_COMM_WORLD
                )
            );
        }
    }
}

static void third_stage(
    Message *message,
    cell *temp,
    const Comm *comm,
    int rank,
    int size
) {
    cell *right_far  = message->right_far;
    cell *right_near = message->right_near;
    cell *left_far   = message->left_far;
    cell *left_near  = message->left_near;
    int count = message->buffer_size;
    if (size == 1) {
        memcpy(left_far, right_near, count*sizeof(cell));
        memcpy(right_far, left_near, count*sizeof(cell));
        return;
    }
    if (rank == 0) {
        if (!comm->send_left) {
            RET_IF_ERR(
                MPI_Send(
                    left_near, count, MPI_CHAR,
                    size - 1, TAG_EDGE, MPI_COMM_WORLD
                )
            );
        }
        if (!comm->recv_left) {
            RET_IF_ERR(
                MPI_Recv(
                    temp, count, MPI_CHAR, 
                    size - 1, TAG_EDGE, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                )
            );
            memcpy(left_far, temp, count*sizeof(cell));
        }
    } else if (rank + 1 == size) {
        if (!comm->recv_right) {
            RET_IF_ERR(
                MPI_Recv(
                    temp, count, MPI_CHAR, 
                    0, TAG_EDGE, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                )
            );
            memcpy(right_far, temp, count*sizeof(cell));
        }
        if (!comm->send_right) {
            RET_IF_ERR(
                MPI_Send(
                    right_near, count, MPI_CHAR,
                    0, TAG_EDGE, MPI_COMM_WORLD
                )
            );
        }
    }
}

static void code_skip_side(int skip, cell *edge) {
    assert(sizeof(int)  == 4);
    assert(sizeof(cell) == 1);
    cell *skip_arr = (cell*)(&skip);
    for (int q = 0; q < 4; ++q) {
        edge[4] += edge[q] << (q + 1);
        edge[q] = skip_arr[q];
    }
}

static int decode_skip_side(cell *edge) {
    assert(sizeof(int)  == 4);
    assert(sizeof(cell) == 1);
    int skip = 0;
    cell *skip_arr = (cell*)(&skip);
    for (int q = 0; q < 4; ++q) {
        skip_arr[q] = edge[q];
        edge[q] = (edge[4] >> (q + 1)) & 1;
    }
    edge[4] = edge[4] & 1;
    return skip;
}

static void code_skip(Message *message) {
    code_skip_side(message->skip_left, message->left_near);
    code_skip_side(message->skip_right, message->right_near);
}

static void decode_skip(Message *message, int skips[2]) {
    skips[0] = decode_skip_side(message->left_far);
    skips[1] = decode_skip_side(message->right_far);
    decode_skip_side(message->left_near);
    decode_skip_side(message->right_near);
}


void exchange_edge_mpi(Message *message, void *adat, const Index *index) {
    int rank = index->rank;
    int size = index->rank_count;
    cell *temp_buffer = (cell*) malloc(message->buffer_size*sizeof(cell));
    static Comm comm =  {
        .recv_left  = 0,
        .send_left  = 0,
        .recv_right = 0,
        .send_right = 0
    };
    code_skip(message);
    first_stage(message, temp_buffer, &comm, rank, size);
    second_stage(message, temp_buffer, &comm, rank, size);
    third_stage(message, temp_buffer, &comm, rank, size);
    int skips[2];
    decode_skip(message, skips);
    
    comm_set_new_val(&comm, message, skips);
    comm_timer_step(&comm);

    free(temp_buffer);
}

void print_mpi(const struct Game_t *game, const Index *index) {
    const int main_rank = 0;
    int rank = index->rank;
    int size = index->rank_count;
    int grid_size = game->w * game->h;
    cell *buffer = NULL;
    if (rank == main_rank) {
        buffer = (cell*) malloc(index->rank_count * grid_size
                                                        * sizeof(cell));
        // This does not look good    
    }
    RET_IF_ERR(
        MPI_Gather(
            game->grid, grid_size, MPI_CHAR,
            buffer, grid_size, MPI_CHAR,
            main_rank, MPI_COMM_WORLD
        )
    );
    if (rank == main_rank) {
        for (int y = 0; y < game->h; ++y) {
            for (int proc = 0; proc < index->rank_count; ++proc) {
                for (int x = 1; x + 1 < game->w; ++x) {
                    if ((buffer + grid_size*proc)[x * game->h + y]) {
                        printf("#");
                    } else {
                        printf("_");
                    }
                }
            }
            printf("\n");
        }
    }
    if (rank == main_rank) {
        free(buffer);
    }
}

// --------------------------------------------------------- Hybrid version

typedef struct Adat_nybryd_t {
    pthread_barrier_t barrier;
} Adat_nybryd;

void exchange_edge_in_thread(Message *message, const Index *index) {
    if (index->rank == 0) {
        cell *grid = message->game->grid;
        int edje_size = message->buffer_size;
        int w = message->game->w;
        int h = message->game->h;
        for (int q = 0; q < index->rank_count; ++q) {
            cell *self_right = message->right_near + w*h*q;
            cell *self_left  = message->left_near  + w*h*q;
            cell *left_right = message->right_far  + w*h*(q-1);
            cell *right_left = message->left_far   + w*h*(q+1);
            if (q != 0) {
                memcpy(left_right, self_left, edje_size);
            }
            if (q + 1 != index->rank_count) {
                memcpy(right_left, self_right, edje_size);
            }
        }
    }
}

void exchange_edge_between(Message *message, const Index *index) {
    int w = message->game->w;
    int h = message->game->h;
    cell *right_near = message->right_near + w*h*(index->rank_count - 1);
    cell *right_far  = message->right_far  + w*h*(index->rank_count - 1);
    cell *left_near  = message->left_near;
    cell *left_far   = message->left_far;
    int count = message->buffer_size;
    const int TAG_SEND_EDJE_HYBRID = 1;
    if (index->rank != 0) {
        return;
    }
    if (index->node == 0) {
        RET_IF_ERR(
            MPI_Send(
                right_near, count, MPI_CHAR,
                1, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD
            )
        );
        RET_IF_ERR(
            MPI_Send(
                left_near, count, MPI_CHAR,
                1, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD
            )
        );
        RET_IF_ERR(
            MPI_Recv(
                left_far, count, MPI_CHAR,
                1, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        RET_IF_ERR(
            MPI_Recv(
                right_far, count, MPI_CHAR,
                1, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
    } else {
        RET_IF_ERR(
            MPI_Recv(
                left_far, count, MPI_CHAR,
                0, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        RET_IF_ERR(
            MPI_Recv(
                right_far, count, MPI_CHAR,
                0, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        RET_IF_ERR(
            MPI_Send(
                right_near, count, MPI_CHAR,
                0, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD
            )
        );
        RET_IF_ERR(
            MPI_Send(
                left_near, count, MPI_CHAR,
                0, TAG_SEND_EDJE_HYBRID, MPI_COMM_WORLD
            )
        );
    }
}

void exchange_edge_hybrid(Message *message, void *adat, const Index *index) {
    int rank = index->node;
    int size = index->node_count;
    cell *temp_buffer = (cell*) malloc(message->buffer_size*sizeof(cell));
    static Comm comm =  {
        .recv_left  = 0,
        .send_left  = 0,
        .recv_right = 0,
        .send_right = 0
    };
    // code_skip(message);
    // sleep(1);
    Adat_nybryd * adath =(Adat_nybryd*) adat;
    pthread_barrier_wait(&adath->barrier);

    exchange_edge_in_thread(message, index);
    exchange_edge_between(message, index);

    pthread_barrier_wait(&adath->barrier);

    free(temp_buffer);
}

void print_hybrid(const struct Game_t *game, const Index *index) {
    if (index->rank == 0) {
        const int main_node = 0;
        int node = index->node;
        int size = index->node_count;
        int w = game->w;
        int h = game->h;
        cell *grid = game->grid;
        int grid_size = w * h;
        int whole_grid_size = w * index->rank_count * h;
        cell *buffer = NULL;
        if (node == main_node) {
            buffer = (cell*) malloc(size * whole_grid_size * sizeof(cell));
            // This does not look good
        }
        RET_IF_ERR(
            MPI_Gather(
                grid, whole_grid_size, MPI_CHAR,
                buffer, whole_grid_size, MPI_CHAR,
                main_node, MPI_COMM_WORLD
            )
        );
        if (node == main_node) {
            for (int y = 0; y < h; ++y) {
                for (int proc = 0; proc < size; ++proc) {
                    for (int th = 0; th < index->rank_count; ++th) {
                        for (int x = 0; x < w; ++x) {
                            if (
                                *(buffer
                                    + whole_grid_size*proc
                                    + (w*th + x)*h
                                    + y
                                )) {
                                printf("#");
                            } else {
                                printf("_");
                            }
                        }
                        printf(" ");
                    }
                    printf("| ");
                }
                printf("\n");
            }
            printf("\n\n");
        }
        if (node == main_node) {
            free(buffer);
        }
    }
}

// --------------------------------------------------------- main functions

int main_sequantial(int argc, char **argv, int ws, int hs, int count) {

    Index index = index_init(0, 1, 0, 1);
    Game game = game_init(
        ws,
        hs,
        index,
        start_time,
        exchange_edge,
        print_sequential,
        end_time,
        exchenge_time,
        NULL
    );

    game_start_game_loop(&game);

    return 0;
}

int main_mpi(int argc, char **argv, int ws, int hs, int count) {

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Index index = index_init(0, 1, rank, size);
    Game game = game_init(
        ws,
        hs/size,
        index,
        start_time,
        exchange_edge_mpi,
        print_mpi,
        end_time,
        exchenge_time,
        NULL
    );

    game_start_game_loop(&game);

    MPI_Finalize();

    return 0;
}

typedef struct ThreadData_t {
    Index index;
    cell *grid;
    int w;
    int h;
    Adat_nybryd *adat;
} ThreadData;

void *thread_function(void *data_void) {
    ThreadData *data = (ThreadData*) data_void;

    Game game = game_init_with_grid(
        data->w,
        data->h,
        data->index,
        data->grid,
        start_time,
        exchange_edge_hybrid,
        print_hybrid,
        end_time,
        exchenge_time,
        (void*) data->adat
    );

    game_start_game_loop(&game);

    game_dstr(&game);

    return NULL;
}

int main_hybrid(int argc, char **argv, int ws, int hs, int count) {

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    assert(size == 2);

    int thread_grid_width = ws/2/count;
    int threads_per_node = count;
    int w = (thread_grid_width + 2) * threads_per_node;
    int h = hs;
    cell *grid = (cell*) malloc(h * w * sizeof(cell));
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            grid[x * h + y] = CELL_DEAD;
        }
    }
    if (rank == 0) {
        draw_lwss(grid, w, h);
    }

    Adat_nybryd adat;
    int err = pthread_barrier_init(&adat.barrier, NULL, threads_per_node);
    assert(err == 0);

    pthread_t *thread_arr = (pthread_t*) malloc(threads_per_node *
                                                    sizeof(pthread_t));
    ThreadData *data_arr = (ThreadData*) malloc(threads_per_node * 
                                                    sizeof(ThreadData));
    for (int q = 0; q < threads_per_node; ++q) {
        data_arr[q] = (ThreadData) {
            .index = index_init(rank, size, q, threads_per_node),
            .grid  = grid + (thread_grid_width + 2) * h * q,
            .w = thread_grid_width + 2,
            .h = h,
            .adat = &adat
        };
        if (q != 0) {
            pthread_create(
                &(thread_arr[q]),
                NULL,
                thread_function,
                (void*) (&data_arr[q])
            );
        }
    }
    thread_function((void*) data_arr);

    free(thread_arr);
    free(data_arr);

    MPI_Finalize();

    return 0;
}

int main(int argc, char **argv) {
    int ws = 1000;
    int hs = 100;
    int count = GCOUNT;
    #if GTYPE == 0
        return main_sequantial(argc, argv, ws, hs, count);
    #elif GTYPE == 1
        return main_mpi(argc, argv, ws, hs, count);
    #elif GTYPE == 2
        return main_hybrid(argc, argv, ws, hs, count);
    #endif
}
