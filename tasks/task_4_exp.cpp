#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <assert.h>
#include "../slibs/err_proc.h"
// #include "mpich/mpi.h"



class Decimal {
    static_assert(sizeof(uint32_t) == 4);
    static_assert(sizeof(uint64_t) == 8);

    std::size_t size = 0;
    uint32_t *arr;
    std::size_t point = 0;
    bool is_positive = true;
    // static constexpr uint32_t base = 1'000'000'000;
    // static constexpr uint32_t base_len = 8;
    static constexpr uint32_t base = 10'000;
    static constexpr uint32_t base_len = 4;

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
            std::cout << size << std::endl;
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
            if (size != other.size) {
                assert(false);
                return *this;
            }

            bool carry;
            for (std::size_t q = size - 1;; --q) {
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
            uint32_t* buf = new uint32_t[size+1];

            for (int q = 0; q < size; ++q) {
                for (int w = 0; w < size; ++w) {
                    if (q + w < size+1) {
                        buf[q + w] += arr[q] * other.arr[w];
                    }
                }
            }
        
            for (int q = size; q > 0; --q) {
                buf[q - 1] += buf[q] / base;
                buf[q] %= base;
            }

            for (int q = 0; q < size; ++q) {
                arr[q] = buf[q];
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

        friend std::ostream& operator<<(std::ostream& out,
                                                    const Decimal& num) {
            out << num.arr[0] << '.';
            for (std::size_t q = 1; q < num.size; ++q) {
                out << std::setw(base_len) << std::setfill('0')
                                                    << num.arr[q] << "";
            }

            return out;
        }

        std::size_t get_size() {
            return size;
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

void calc_proc(uint32_t start, uint32_t end, std::size_t prec, int rank) {
    Decimal accumulator{1, prec};
    Decimal devisible{1, prec};
    // std::cout << rank << "-> " << devisible << std::endl;

    for (uint32_t q = start; q < end; ++q) {
        devisible /= q;
        // std::cout << rank << "-> " << devisible << std::endl;
        accumulator += devisible;
    }

    // std::cout << rank << "-> " << accumulator << std::endl;
    std::cout << accumulator << std::endl;
    // std::cout << rank << "-> " << accumulator.get_size() << std::endl;
}

double a(std::size_t n) {
    return std::log10(M_2_PI * n) + n * std::log10(n/M_E);
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
    std::cout << dec_a << "[8.539734222673566]" << std::endl;
    std::cout << dec_a << "[8.539734222673565677848730527685]" << std::endl;

    return;
}

int main(int argc, char** argv) {
    // RET_IF_ERR(MPI_Init(&argc, &argv));

    // test_decimal();

    RET_IF_ERR(argc != 2);

    // int size, rank;
    int size = 1, rank = 0;
    // RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    // RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    // printf("Hello world!\n");
    // printf("size: %d\n", size);
    // printf("rank: %d\n", rank);

    // int N = atoi(argv[1]);
    // RET_IF_ERR(N);
    // int N = 5047;
    int prec = atoi(argv[1]) / Decimal::get_base_len();
    int N = 3*prec + 1;
    int from = N/size*rank + 1;
    int to = (rank + 1 == size) ? (N) : (from + N/size);
    // std::cout << rank << " -> " << from << "-" << to << std::endl;
    // calc_proc(from, to, prec, rank);
    calc_proc(1, N, prec, rank);

    // RET_IF_ERR(MPI_Finalize());

    return 0;
}
