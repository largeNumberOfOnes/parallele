#include <assert.h>
#include <limits.h>
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
    int skip_left;
    int skip_right;
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
    int skip_right;
    int skip_left;
    void (*start_time)    (Time *time, const Index *index);
    void (*exchange_edge) (Message *message, const Index *index);
    void (*print)         (const struct Game_t *game, const Index *index);
    void (*end_time)      (Time *time, const Index *index);
    void (*exchenge_time) (Time *time, const Index *index);
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
    game->skip_left  = skip_left;
    game->skip_right = skip_right;
    // printf("real rank: %d -> (%d, %d) = %d\n", game->index.rank, skip_left, skip_right, 28 - skip_left - skip_right);
}

void game_start_game_loop(Game *game) {
    Time time = time_init();
    game->print(game, &game->index);
    while (1) {
        // game->start_time(&time);
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
            .buffer_size = game->h
        };
        game->exchange_edge(&message, &game->index);
        usleep(100000);
        game->print(game, &game->index);
        // game->end_time(&time);
        // game->exchenge_time(&time);
        usleep(100000);
        if (game->index.rank == 0) { DOT }
    }
}

// ------------------------------------------------------------------------

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
    if (comm->recv_left  == 0) { comm->recv_left  = MAX(skips[0]            - 1, 0); }
    if (comm->send_left  == 0) { comm->send_left  = MAX(message->skip_left  - 1, 0); }
    if (comm->recv_right == 0) { comm->recv_right = MAX(skips[1]            - 1, 0); }
    if (comm->send_right == 0) { comm->send_right = MAX(message->skip_right - 1, 0); }
    // comm->recv_left =  (comm->recv_left  < 0) ? 0 : comm->recv_left;
    // comm->send_left =  (comm->send_left  < 0) ? 0 : comm->send_left;
    // comm->recv_right = (comm->recv_right < 0) ? 0 : comm->recv_right;
    // comm->send_right = (comm->send_right < 0) ? 0 : comm->send_right;
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

void code_skip_side(int skip, cell *edge) {
    assert(sizeof(int)  == 4);
    assert(sizeof(cell) == 1);
    cell *skip_arr = (cell*)(&skip);
    // printf("skip    "); for (int q = 0; q < 4; ++q) { printf("%d ", skip_arr[q]); } printf("\n");
    // printf("before  "); for (int q = 0; q < 10; ++q) { printf("%d ", edge[q]); } printf("\n");
    for (int q = 0; q < 4; ++q) {
        edge[4] += edge[q] << (q + 1);
        edge[q] = skip_arr[q];
    }
    // printf("coded   "); for (int q = 0; q < 10; ++q) { printf("%d ", edge[q]); } printf("\n");
}

int decode_skip_side(cell *edge) {
    assert(sizeof(int)  == 4);
    assert(sizeof(cell) == 1);
    int skip = 0;
    cell *skip_arr = (cell*)(&skip);
    // printf("coded  "); for (int q = 0; q < 10; ++q) { printf("%d ", edge[q]); } printf("\n");
    for (int q = 0; q < 4; ++q) {
        skip_arr[q] = edge[q];
        edge[q] = (edge[4] >> (q + 1)) & 1;
    }
    // printf("after  "); for (int q = 0; q < 10; ++q) { printf("%d ", edge[q]); } printf("\n");
    edge[4] = edge[4] & 1;
    return skip;
}

void code_skip(Message *message) {
    code_skip_side(message->skip_left, message->left_near);
    code_skip_side(message->skip_right, message->right_near);
}

void decode_skip(Message *message, int skips[2]) {
    skips[0] = decode_skip_side(message->left_far);
    skips[1] = decode_skip_side(message->right_far);
    decode_skip_side(message->left_near);
    decode_skip_side(message->right_near);
}

void start_time(Time *time, const Index *index) {}
void exchange_edge_sequential (Message *message, const Index *index) {}
void exchange_edge_mpi (Message *message, const Index *index) {
    int rank = index->rank;
    int size = index->rank_count;
    cell *temp_buffer = (cell*) malloc(message->buffer_size*sizeof(cell));
    static Comm comm =  {
        .recv_left  = 0,
        .send_left  = 0,
        .recv_right = 0,
        .send_right = 0
    };
    // printf("%d -> %d ", rank, comm.recv_left); printf("%d ", comm.send_left); printf("%d ", comm.recv_right); printf("%d\n", comm.send_right);
    // for (int q = 0; q < 10; ++q) { printf("%d ", message->left_near[q]); } printf("\n");
    // printf("rank: %d -> (%d, %d)\n", index->rank, message->skip_left, message->skip_right);
    code_skip(message);
    first_stage(message, temp_buffer, &comm, rank, size);
    second_stage(message, temp_buffer, &comm, rank, size);
    third_stage(message, temp_buffer, &comm, rank, size);
    int skips[2];
    decode_skip(message, skips);
    // printf("%d -> mes  : %d %d\n", rank, message->skip_left, message->skip_right);
    // printf("%d -> skips: %d %d\n", rank, skips[0], skips[1]);

    
    comm_set_new_val(&comm, message, skips);
    comm_timer_step(&comm);
    // printf("%d -> %d ", rank, comm.recv_left);
    // printf("%d ", comm.send_left);
    // printf("%d ", comm.recv_right);
    // printf("%d\n", comm.send_right);
    // printf("rank: %d -> (%d, %d)\n", index->rank, skips[0], skips[1]);

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
                // for (int x = 0; x < game->w; ++x) {
                    if ((buffer + grid_size*proc)[x * game->h + y]) {
                        printf("#");
                    } else {
                        if (x == 1 || x == game->w - 2) {
                            // printf("'");
                            printf("_");
                        } else {
                            printf("_");
                        }
                    }
                }
                // printf(" ");
            }
            printf("\n");
        }
    }
}
void end_time(Time *time, const Index *index) {}
void exchenge_time(Time *time, const Index *index) {}

// --------------------------------------------------------- main functions

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
