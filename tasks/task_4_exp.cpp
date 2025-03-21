#include <cctype>
// #include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <stdint.h>
#include <thread>
#include "../slibs/err_proc.h"
#include "mpi.h"
// #include "mpi_proto.h"
// #include "mpich/mpi.h"

constexpr int UPPER_SUM_TAG = 1;
// constexpr int MUL_DEVIS_TAG = 0;
// constexpr int MUL_ACCUM_TAG = 0;

class Decimal {
    static_assert(sizeof(uint32_t) == 4);
    static_assert(sizeof(uint64_t) == 8);

    std::size_t size = 0;
    uint32_t *arr;
    std::size_t point = 0;
    bool is_positive = true;
    static constexpr uint32_t base = 1'000'000'000;
    static constexpr uint32_t base_len = 9;
    // static constexpr uint32_t base = 10'000;
    // static constexpr uint32_t base_len = 4;

    static constexpr std::size_t comp_size = sizeof(long unsigned int);

    public:
        Decimal(uint32_t a, std::size_t digits, bool is_positive = true)
            // : size(1 + (digits / 8 + 1) + ((digits % 8 > 5) ? (1) : (0)))
            : size(digits)
            , arr(new uint32_t[size])
            , point(size - 2)
            , is_positive(is_positive)
        {
            for (int q = 0; q < size; ++q) {
                arr[q] = 0;
            }
            arr[0] = a;
        }

        int dti(char digit) {
            return digit - '0';
        }

        Decimal(const char* str) {
            assert(str);
            std::size_t len = std::strlen(str);
            // assert(!std::isdigit(str[0]) || str[0] == '-');
            // for (std::size_t q = 1; q < len; ++q) {
            //     assert(!std::isdigit(str[q]) || str[0] == '-');
            // }

            is_positive = str[0] == '-';

            int comma_pos = 0;
            uint32_t ved = 0;
            while (str[++comma_pos] != '.') {}
            uint32_t mult = 1;
            for (int q = comma_pos; q > 0; --q) {
                ved += dti(str[q-1]) * mult;
                mult *= 10;
            }
            size = (len - comma_pos - 1) / base_len + 2;
            // std::cout << size << std::endl;
            arr = new uint32_t[size];
            arr[0] = ved;

            for (int q = 1; q < size; ++q) {
                arr[q] = 0;
            }

            for (int q = comma_pos+1; q < len; ++q) {
                if ((q - comma_pos - 1) % base_len == 0) {
                    mult = base;
                }
                mult /= 10;
                arr[1 + ((q - comma_pos - 1) / base_len)] += dti(str[q]) * mult;
            }


        }

        ~Decimal() {
            delete [] arr;
        }

        Decimal(const Decimal&) = delete;
        Decimal(const Decimal&&) = delete;
        void operator=(const Decimal&) = delete;
        void operator=(const Decimal&&) = delete;

        Decimal& operator+=(Decimal& other) {
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

            for (int q = 0; q < size; ++q) {
                for (int w = 0; w < size; ++w) {
                    if (q + w < size + 1) {
                        buf[q + w] += static_cast<uint64_t>(arr[q]) * static_cast<uint64_t>(other.arr[w]);
                    }
                }
            }
        
            for (int q = size; q > 0; --q) {
                buf[q - 1] += buf[q] / static_cast<uint64_t>(base);
                buf[q] %= static_cast<uint64_t>(base);
            }

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
                divisible = (reminder * static_cast<uint64_t>(base)) + static_cast<uint64_t>(arr[q]);
                arr[q] = divisible / static_cast<uint64_t>(divider);// % base;
                reminder = divisible % static_cast<uint64_t>(divider);
                // std::cout << arr[q] << " | " << reminder << " | " << divisible << std::endl;
            }

            return *this;
        }

        Decimal& divide__(uint32_t divider, int &start) {
            uint64_t reminder = 0;
            uint64_t divisible = 0;
            bool is_prev_z = true;
            for (std::size_t q = start; q < size; ++q) {
                divisible = (reminder * static_cast<uint64_t>(base)) + static_cast<uint64_t>(arr[q]);
                arr[q] = divisible / static_cast<uint64_t>(divider);// % base;
                reminder = divisible % static_cast<uint64_t>(divider);
                // std::cout << arr[q] << " | " << reminder << " | " << divisible << std::endl;
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

        [[nodiscard]] uint32_t* get_arr() {
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

void test_decimal() {
    Decimal dec{1, 14, true};
    Decimal dec2{9, 14, true};

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

    // Decimal dec_a{"3.141592653589793"};
    // Decimal dec_b{"2.718281828459045"};
    // std::cout << dec << "[?]" << std::endl;
}

void calc_part(
    Decimal& accumulator,
    Decimal& devisible,
    uint32_t start,
    uint32_t end,
    std::size_t prec,
    int rank
) {
    // std::this_thread::sleep_for(std::chrono::seconds(rank));
    // std::cout << rank << "-> " << devisible << std::endl;

    for (uint32_t q = start; q < end; ++q) {
        devisible /= q;
        accumulator += devisible;
        // std::cout << rank << "-> " << devisible << std::endl;
        // std::cout << rank << "-> " << accumulator << std::endl;
        // std::cout << rank << "-> " << q << " " << devisible << " " << accumulator << std::endl;
        // if (q % 20 == 0 && devisible.is_null()) {
        // if (devisible.is_null()) {
            // break;
        // }
    }

    // std::cout << rank << "-> " << accumulator << std::endl;
    // std::cout << std::endl;
    // if (rank == 0) {
    //     std::cout << rank << " -> devisible: " << devisible << std::endl;
    //     std::cout << rank << " -> accumulator: " << accumulator << std::endl;
    // }
    // std::cout << rank << "-> " << accumulator.get_size() << std::endl;

}

int calc_N(std::size_t digits) {
    int N = 2;
    int start = 0;
    Decimal devisible{1, digits};
    while (true) {
        devisible.divide__(N, start);
        // std::cout << devisible << std::endl;
        // std::cout << "N: " << N << std::endl;
        if (devisible.is_null()) {
            break;
        }
        ++N;
    }
    // std::cout << devisible << std::endl;
    return N;
}

int calc_proc(uint32_t start, uint32_t end, std::size_t prec, int rank, int size) {
    Decimal accumulator{0, prec};
    Decimal devisible{1, prec};
    calc_part(accumulator, devisible, start, end, prec, rank);
    // std::cout << rank << " -> " << accumulator << std::endl;
    // Decimal prev_devisible{1, prec};
    Decimal upper_sum{0, prec};

    std::size_t dec_size = devisible.get_size() * devisible.get_type_size();
    // std::cout << "dec_size" << dec_size << std::endl;

    if (rank + 1 != size) {
        RET_IF_ERR(
            MPI_Recv(
                upper_sum.get_arr(),
                dec_size,
                MPI_CHAR, // MPI_BYTE,
                rank + 1,
                UPPER_SUM_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        // std::cout << rank << " -> receive: " << upper_sum << std::endl;
        upper_sum *= devisible;
        // std::cout << rank << " -> dev: " << devisible << std::endl;
        // std::cout << rank << " -> usm: " << upper_sum << std::endl;
        accumulator += upper_sum;
    }
    if (rank != 0) {
        RET_IF_ERR(
            MPI_Send(
                accumulator.get_arr(),
                dec_size,
                MPI_CHAR, // MPI_BYTE,
                rank - 1,
                UPPER_SUM_TAG, MPI_COMM_WORLD
            )
        );
    } else {
        accumulator += 1;
        // std::cout << rank << " -> final: " << accumulator << std::endl;
        std::ofstream output("ret_e.txt");
        output << accumulator << std::endl;
        // std::cout << accumulator << std::endl;
    }

    return 0;
}

// double a(std::size_t n) {
//     return std::log10(M_2_PI * n) + n * std::log10(n/M_E);
// }

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
    // std::cout << dec_a << "[8.539734222673566]" << std::endl;
    std::cout << dec_a << "[8.539734222673565677848730527685]" << std::endl;

    return;
}

int main2(int argc, char** argv) {
    test_mil_long_long();
    return 0;
}

int main(int argc, char** argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    RET_IF_ERR(argc != 2);

    // int size, rank;
    int size = 1, rank = 0;
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    int prec = atoi(argv[1]);
    int digits = prec / Decimal::get_base_len() + 2;
    int N = 0;
    if (rank == 0) {
        std::cout << "alg: " << 3*prec + 2 << std::endl;
        N = calc_N(digits);
        std::cout << "cal: " << N << std::endl;
    }
    // int N = 3*prec + 2;
    // N = 3*prec + 2;
    // N = 6;
    int from = 1 + N/size*rank;
    // int to = (rank + 1 == size) ? (N) : (1 + N/size*(rank+1));
    int to = (rank + 1 == size) ? (N+1) : (1 + N/size*(rank+1));
    // std::cout << rank << " -> " << from << "-" << to << std::endl;
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    calc_proc(from, to, digits, rank, size);
    // calc_proc(1, N, digits, rank, size);

    RET_IF_ERR(MPI_Finalize());

    return 0;
}
