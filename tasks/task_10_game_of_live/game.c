#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "../../slibs/err_proc.h"
#include "mpi_proto.h"
// #include "mpi_proto.h"



const int TAG_EDGE = 1;

typedef char cell;
const cell CELL_DEAD    = 0;
const cell CELL_ALIVE   = 1;
const cell CELL_DYING   = 2;
const cell CELL_NEWBORN = 3;

typedef struct Message_t {
    int skip;
    int buffer_size;
    cell *left_far;
    cell *left_near;
    cell *right_far;
    cell *right_near;
} Message;

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

void draw_glider(cell *grid, int w, int h) {
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
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            // grid[y * w + x] = (rand() % 20 == 7) ? CELL_ALIVE : CELL_DEAD;
            grid[x * h + y] = CELL_DEAD;
        }
    }
    if (index.rank == 0) {
        // for (int x = 3; x < 6; ++x) {
        //     grid[x * h + 0] = CELL_ALIVE;
        // }
        draw_glider(grid, w, h);
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
        int w = game->w;
        int h = game->h;
        Message message = {
            .skip = 0,
            .left_far   = game->grid,
            .left_near  = game->grid + h,
            .right_far  = game->grid + h * (w - 1),
            .right_near = game->grid + h * (w - 2),
            .buffer_size = game->h
        };
        game->exchange_edge(&message, &game->index);
        evalute(game);
        // game->end_time(&time);
        // game->exchenge_time(&time);
        DOT
        usleep(300000);
    }
}

// ------------------------------------------------------------------------

static void first_stage(
    Message *message,
    cell *temp,
    int rank,
    int size
) {
    cell *right_far  = message->right_far;
    cell *right_near = message->right_near;
    cell *left_far   = message->left_far;
    cell *left_near  = message->left_near;
    int count = message->buffer_size;
    if (rank % 2 == 0 && rank + 1 != size) {
        RET_IF_ERR(
            MPI_Send(
                right_near, count, MPI_CHAR,
                rank + 1, TAG_EDGE, MPI_COMM_WORLD
            )
        );
        RET_IF_ERR(
            MPI_Recv(
                temp, count, MPI_CHAR, 
                rank + 1, TAG_EDGE, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            )
        );
        memcpy(right_far, temp, count*sizeof(cell));
    } else if (rank % 2 == 1) {
        RET_IF_ERR(
            MPI_Recv(
                temp, count, MPI_CHAR, 
                rank - 1, TAG_EDGE, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            )
        );
        RET_IF_ERR(
            MPI_Send(
                left_near, count, MPI_CHAR,
                rank - 1, TAG_EDGE, MPI_COMM_WORLD
            )
        );
        memcpy(left_far, temp, count*sizeof(cell));
    }
}

static void second_stage(
    Message *message,
    cell *temp,
    int rank,
    int size
) {
    cell *right_far  = message->right_far;
    cell *right_near = message->right_near;
    cell *left_far   = message->left_far;
    cell *left_near  = message->left_near;
    int count = message->buffer_size;
    if (rank % 2 == 0 && rank != 0) {
        RET_IF_ERR(
            MPI_Send(
                left_near, count, MPI_CHAR,
                rank - 1, TAG_EDGE, MPI_COMM_WORLD
            )
        );
        RET_IF_ERR(
            MPI_Recv(
                temp, count, MPI_CHAR, 
                rank - 1, TAG_EDGE, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            )
        );
        memcpy(left_far, temp, count*sizeof(cell));
    } else if (rank % 2 == 1 && rank + 1 != size) {
        RET_IF_ERR(
            MPI_Recv(
                temp, count, MPI_CHAR, 
                rank + 1, TAG_EDGE, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            )
        );
        RET_IF_ERR(
            MPI_Send(
                right_near, count, MPI_CHAR,
                rank + 1, TAG_EDGE, MPI_COMM_WORLD
            )
        );
        memcpy(right_far, temp, count*sizeof(cell));
    }
}

static void third_stage(
    Message *message,
    cell *temp,
    int rank,
    int size
) {
    cell *right_far  = message->right_far;
    cell *right_near = message->right_near;
    cell *left_far   = message->left_far;
    cell *left_near  = message->left_near;
    int count = message->buffer_size;
    if (rank == 0) {
        RET_IF_ERR(
            MPI_Send(
                left_near, count, MPI_CHAR,
                size - 1, TAG_EDGE, MPI_COMM_WORLD
            )
        );
        RET_IF_ERR(
            MPI_Recv(
                temp, count, MPI_CHAR, 
                size - 1, TAG_EDGE, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            )
        );
        memcpy(left_far, temp, count*sizeof(cell));
    } else if (rank + 1 == size) {
        RET_IF_ERR(
            MPI_Recv(
                temp, count, MPI_CHAR, 
                0, TAG_EDGE, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            )
        );
        RET_IF_ERR(
            MPI_Send(
                right_near, count, MPI_CHAR,
                0, TAG_EDGE, MPI_COMM_WORLD
            )
        );
        memcpy(right_far, temp, count*sizeof(cell));
    }
}

void start_time(Time *time, const Index *index) {}
void exchange_edge_sequential (Message *message, const Index *index) {}
void exchange_edge_mpi (Message *message, const Index *index) {
    int rank = index->rank;
    int size = index->rank_count;
    cell *temp_buffer = (cell*) malloc(message->buffer_size*sizeof(cell));
    
    first_stage(message, temp_buffer, rank, size);
    second_stage(message, temp_buffer, rank, size);
    third_stage(message, temp_buffer, rank, size);

    free(temp_buffer);
}
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
void print_mpi(const struct Game_t *game, const Index *index) {
    const int main_rank = 0;
    int rank = index->rank;
    int size = index->rank_count;
    int grid_size = game->w * game->h;
    cell *buffer = NULL;
    if (rank == main_rank) {
        buffer = (cell*) malloc(index->rank_count * grid_size * sizeof(cell));
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
}
void end_time(Time *time, const Index *index) {}
void exchenge_time(Time *time, const Index *index) {}

// ------------------------------------------------------------------------


int main_sequantial(int argc, char **argv) {

    Index index = index_init(0, 1, 0, 1);
    Game game = game_init(
        10,
        10,
        index,
        start_time,
        exchange_edge_sequential,
        print_sequential,
        end_time,
        exchenge_time
    );

    game_start_game_loop(&game);

    return 0;
}

int main_mpi(int argc, char **argv) {

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Index index = index_init(0, 1, rank, size);
    Game game = game_init(
        30,
        20,
        index,
        start_time,
        exchange_edge_mpi,
        print_mpi,
        end_time,
        exchenge_time
    );

    game_start_game_loop(&game);

    MPI_Finalize();

    return 0;
}

int main(int argc, char **argv) {
    // return main_sequantial(argc, argv);
    return main_mpi(argc, argv);
}
