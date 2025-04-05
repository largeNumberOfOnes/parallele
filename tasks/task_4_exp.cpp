/*
 * This program counts the number e to the nearest N. N is specified by
 *    the first argument of the program. It is mandatory. In the result
 *    this programs creates file ret_e.txt this first N digits of e.
 *    Second arument of program is optinal. If this one is "apr" the
 *    program will use aproximation (digits = 3*N + 1) else digits count
 *    will be calulated before the program start.
 */

#include <cstring>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <assert.h>
#include "mpi.h"
#include "../slibs/err_proc.h"



constexpr int UPPER_SUM_TAG = 1;

class Decimal {
    static_assert(sizeof(uint32_t) == 4);
    static_assert(sizeof(uint64_t) == 8);

    static constexpr uint32_t base = 1'000'000'000;
    static constexpr uint32_t base_len = 9;

    std::size_t size = 0;
    uint32_t *arr = nullptr;

    public:
        Decimal(uint32_t a, std::size_t digits)
            : size(digits)
            , arr(new uint32_t[size])
        {
            for (int q = 0; q < size; ++q) {
                arr[q] = 0;
            }
            arr[0] = a;
        }

        static int dti(char digit) {
            return digit - '0';
        }

        Decimal(const char* str) {
            std::size_t len = std::strlen(str);

            int comma_pos = 0;
            uint32_t ved = 0;
            while (str[++comma_pos] != '.') {}
            uint32_t mult = 1;
            for (int q = comma_pos; q > 0; --q) {
                ved += dti(str[q-1]) * mult;
                mult *= 10;
            }
            size = (len - comma_pos - 1) / base_len + 2;
            
            arr = new uint32_t[size];
            arr[0] = ved;

            for (int q = 1; q < size; ++q) {
                arr[q] = 0;
            }

            for (int q = comma_pos + 1; q < len; ++q) {
                if ((q - comma_pos - 1) % base_len == 0) {
                    mult = base;
                }
                mult /= 10;
                arr[1 + ((q - comma_pos - 1) / base_len)] +=
                                                dti(str[q]) * mult;
            }
        }

        ~Decimal() {
            delete [] arr;
        }

        Decimal(const Decimal&) = delete;
        Decimal(const Decimal&&) = delete;
        void operator=(const Decimal&) = delete;
        void operator=(const Decimal&&) = delete;

        Decimal& operator+=(const Decimal& other) {
            assert(size == other.size);

            uint64_t carry = 0;
            for (std::size_t q = size - 1; q != -1; --q) {
                arr[q] += other.arr[q] + carry;
                if (arr[q] >= base) {
                    arr[q] -= base;
                    carry = true;
                } else {
                    carry = false;
                }
                if (!q) {
                    break;
                }
            }

            return *this;
        }

        Decimal& operator+=(uint32_t other) {
            arr[0] += other;
            return *this;
        }

        Decimal& operator*=(uint32_t mul) {
            uint32_t carry = 0;
            uint64_t extension = 0;
            for (std::size_t q = size - 1;; --q) {
                extension = static_cast<uint64_t>(arr[q]) * mul + carry;
                if (extension >= base) {
                    carry = extension / base;
                }
                arr[q] = extension % base;
                if (!q) {
                    break;
                }
            }

            return *this;
        }

        Decimal& operator*=(const Decimal& other) {
            uint64_t* buf = new uint64_t[size+1];
            for (int q = 0; q < size + 1; ++q) {
                buf[q] = 0;
            }

            for (int k = size; k > 0; --k) {
                buf[k-1] += buf[k] / base;
                buf[k] %= base;
                for (int q = 1; q <= k; ++q) {
                    uint64_t a = static_cast<uint64_t>(arr[q]);
                    uint64_t b = static_cast<uint64_t>(other.arr[k-q]);
                    buf[k] += a * b;
                    buf[k-1] += buf[k] / base;
                    buf[k] %= base;
                }
            }
            buf[0] %= static_cast<uint64_t>(base);

            for (int q = 0; q < size; ++q) {
                arr[q] = static_cast<uint32_t>(buf[q]);
            } 
            delete [] buf;
            return *this;
        }

        Decimal& operator/=(uint32_t divider) {
            uint64_t reminder = 0;
            uint64_t divisible = 0;
            for (std::size_t q = 0; q < size; ++q) {
                divisible = reminder * base + arr[q];
                arr[q] = divisible / divider;
                reminder = divisible % divider;
            }

            return *this;
        }

        Decimal& divide__(uint32_t divider, int& start) {
            uint64_t reminder = 0;
            uint64_t divisible = 0;
            bool is_prev_z = true;
            for (std::size_t q = start; q < size; ++q) {
                divisible = reminder * base + arr[q];
                arr[q] = divisible / divider;
                reminder = divisible % divider;
                if (arr[q] == 0) {
                    if (is_prev_z) {
                        start = q;
                    }
                } else {
                    is_prev_z = false;
                }
            }

            return *this;
        }

        bool is_null() {
            for (int q = size-1; q >= 0 ; --q) {
                if (arr[q] != 0) {
                    return false;
                }
            }
            return true;
        }

        friend std::ostream& operator<<(std::ostream& out,
                                                    const Decimal& num) {
            out << num.arr[0] << '.';
            for (std::size_t q = 1; q < num.size; ++q) {
                out << std::setw(base_len) << std::setfill('0')
                                                    << num.arr[q] << "";
            }

            return out;
        }

        uint32_t* get_arr() {
            return arr;
        }

        std::size_t get_size() const {
            return size;
        }

        std::size_t get_type_size() const {
            return sizeof(arr[0]);
        }

        static uint32_t get_base() {
            return base;
        }

        static uint32_t get_base_len() {
            return base_len;
        }
};

void calc_part(
    Decimal& accumulator,
    Decimal& devisible,
    uint32_t start,
    uint32_t end,
    std::size_t prec,
    int rank
) {
    int st = 0;
    for (uint32_t q = start; q < end; ++q) {
        devisible.divide__(q, st);
        accumulator += devisible;
    }
}

int calc_N_b_stepping(std::size_t digits) {
    int N = 2;
    int start = 0;
    Decimal devisible{1, digits};
    while (true) {
        devisible.divide__(N, start);
        if (devisible.is_null()) {
            break;
        }
        ++N;
    }
    return N;
}

int calc_proc(
    uint32_t start, uint32_t end,
    std::size_t prec,
    int rank, int size
) {
    Decimal accumulator{0, prec};
    Decimal devisible{1, prec};
    calc_part(accumulator, devisible, start, end, prec, rank);
    Decimal upper_sum{0, prec};
    std::size_t dec_size = devisible.get_size() *
                                                devisible.get_type_size();
    
    if (rank + 1 != size) {
        RET_IF_ERR(
            MPI_Recv(
                upper_sum.get_arr(),
                dec_size,
                MPI_BYTE,
                rank + 1,
                UPPER_SUM_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        upper_sum *= devisible;
        accumulator += upper_sum;
    }
    if (rank != 0) {
        RET_IF_ERR(
            MPI_Send(
                accumulator.get_arr(),
                dec_size,
                MPI_BYTE,
                rank - 1,
                UPPER_SUM_TAG, MPI_COMM_WORLD
            )
        );
    } else {
        accumulator += 1;
        std::ofstream output("output/ret_e.txt");
        output << accumulator << std::endl;
    }

    return 0;
}

void test_decimal() {
    Decimal dec{1, 14};
    Decimal dec2{9, 14};

    std::cout << dec << "[1]" << std::endl;
    dec /= 3;
    std::cout << dec << "[0.(3)]" << std::endl;
    dec += 5;
    std::cout << dec << "[5.(3)]" << std::endl;
    dec += dec2;
    std::cout << dec << "[14.(3)]" << std::endl;
    dec *= 3;
    std::cout << dec << "[43.0]" << std::endl;
    dec2 /= 3;
}

void test_str_to_dec() {
    Decimal dec{"3.141592653589793"};
    std::cout << dec << "[3.141592653589793]" << std::endl;

    return;
}

void test_mil_long_long() {
    Decimal dec_a{"3.141592653589793"};
    Decimal dec_b{"2.718281828459045"};
    std::cout << dec_a << std::endl;
    std::cout << dec_b << std::endl;
    dec_a *= dec_b;
    std::cout << dec_a
              << "[8.539734222673565677848730527685]" << std::endl;

    return;
}

void test_mul_again() {
    for (uint64_t q = 0; q < 1'000'000'000; ++q) {
        if (q % 100'000'000) {
            continue;
        }

        Decimal dec{0, 4};
        for (int w = 0; w < dec.get_size(); ++w) {
            dec.get_arr()[w] = q;
        }
        std::cout << dec << std::endl;
        dec *= dec;
        std::cout << dec << std::endl;

        std::cout << std::endl;
    }

}

int calc_N(int rank, int digits, int argc, char** argv) {
    int N = 0;
    if (argc >= 3 && std::strcmp(argv[2], "apr")) {
        N = 3*digits + 1;
    } else {
        if (rank == 0) {
            N = calc_N_b_stepping(digits);
        }
        RET_IF_ERR(
            MPI_Bcast(
                &N, 1,
                MPI_INT,
                0, MPI_COMM_WORLD
            )
        );
    }

    return N;
}

int main(int argc, char** argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    check_ames(
        argc > 2,
        "You should specify count of digits!"
    );

    int size = 1, rank = 0;
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    int prec = atoi(argv[1]);
    int digits = prec / Decimal::get_base_len() + 2;

    int N = calc_N(rank, digits, argc, argv);
    
    clock_t start, end;
    if (rank == 0) {
        start = clock();
    }

    int from = 1 + N/size*rank;
    int to = (rank + 1 == size) ? (N+1) : (1 + N/size*(rank+1));

    calc_proc(from, to, digits, rank, size);

    if (rank == 0) {
        end = clock();
        double delta_time = (static_cast<double>(end - start))
                                                        / CLOCKS_PER_SEC;
        std::cout << "time: " << delta_time << std::endl;
    }

    RET_IF_ERR(MPI_Finalize());

    return 0;
}
