#include <iostream>
#include <functional>
#include <type_traits>
#include <vector>
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>
#include <chrono>
#include <cmath>
#include <random>
#include <emmintrin.h>
#include <immintrin.h>

using namespace tbb::flow;
using namespace std;

template<typename T>
void benchmark(const T& fun, const std::string& desc = "") {
    if (!desc.empty()) std::cout << "**** " << desc << " ****" << std::endl;

    auto begin = std::chrono::high_resolution_clock::now();
    fun();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur = end - begin;
    std::cout << dur.count() << " sec" << std::endl;
}

vector<double> generate(size_t size) {
    vector<double> data(size);
    mt19937_64 entropy;
    entropy.seed(std::random_device()());
    std::uniform_real_distribution<double> dist;

    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(entropy);
    }

    return data;
}

class FileClose {
public:
    void operator()(FILE* file) {
        fclose(file);
    }
};

typedef unique_ptr<FILE, FileClose> file_guard;

void save_double_list(FILE* file, vector<double>& data) {
    for (double num : data) {
        if (fwrite(&num, sizeof(num), 1, file) != 1) {
            throw std::runtime_error("failed to write");
        }
    }
}

size_t get_file_size(FILE* file) {
    if (fseek(file, 0, SEEK_END) != 0) throw std::runtime_error("failed to seek");
    size_t size = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) throw std::runtime_error("failed to seek");

    return size;
}

vector<double> load_double_list(FILE* file) {
    size_t size = get_file_size(file);
    if (size % sizeof(double) != 0) throw std::runtime_error("file size inconsistent");
    size_t numDataPts = size / sizeof(double);

    vector<double> ret(numDataPts);
    if (fread(&ret[0], sizeof(double), numDataPts, file) != numDataPts) {
        throw std::runtime_error("failed to read");
    }

    return ret;
}

void generate_file(const string& file) {
    file_guard f(fopen(file.c_str(), "wb"));
    if (!f) throw std::runtime_error("failed open file");

    auto data = generate(1 << 27);
    save_double_list(f.get(), data);
}

vector<double> load_file(const string& file) {
    file_guard f(fopen(file.c_str(), "rb"));
    if (!f) throw std::runtime_error("failed open file");

    return load_double_list(f.get());
}



/* ####################################################################
 *
 *
 *   all kinds of sqrt implementations
 *
 *
 * ####################################################################*/

void baseline(vector<double>& num, vector<double>& out) {
    for (size_t i = 0; i < out.size(); i++) {
        out[i] = sqrt(num[i]);
    }
}

inline void sqrt_sse_single(double* out, double* num) {
    __m128d in = _mm_load_sd(num);
    _mm_store_sd(out, _mm_sqrt_sd(in, in));
}


void compare_sse_single(vector<double>& num, vector<double>& out) {
    for (size_t i = 0; i < out.size(); i++) {
        sqrt_sse_single(&out[i], &num[i]);
    }
}

inline void sqrt_sse_double(double* out, double* num) {
    __m128d in = _mm_load_pd(num);
    _mm_store_pd(out, _mm_sqrt_pd(in));
}

void compare_sse_double(vector<double>& num, vector<double>& out) {
    for (size_t i = 0; i < out.size(); i+=2) {
        sqrt_sse_double(&out[i], &num[i]);
    }

    if (out.size() % 2 != 0) out.back() = sqrt(num.back());
}

void check(const vector<double>& base, const vector<double>& result) {
    if (base.size() != result.size()) throw std::runtime_error("not match");

    for (size_t i = 0; i < base.size(); ++i) {
        if (base[i] != result[i]) {
            throw std::runtime_error("not match");
        }
    }
}

inline void sqrt_avx(double* out, double* num) {
    __m256d in = _mm256_load_pd(num);
    _mm256_store_pd(out, _mm256_sqrt_pd(in));
}

void compare_avx(vector<double>& num, vector<double>& out) {
    size_t i = 0;
    for (i = 0; i < out.size(); i+=4) {
        sqrt_avx(&out[i], &num[i]);
    }

    //ignored remains
}

void check_align(void* p) {
    long long ptr  = (long long)p;
    if (ptr % 32 == 0) {
        std::cout << "aligned 32byte" << std::endl;
        return;
    }
    if (ptr % 16 == 0) {
        std::cout << "aligned 16byte" << std::endl;
        return;
    }
    if (ptr % 8 == 0) {
        std::cout << "aligned 8byte" << std::endl;
        return;
    }
}

int main(int argc, char *argv[]) {
    //generate_file("./list.txt");
    vector<double> data = load_file("./list.txt");
    vector<double> ans(data.size());
    vector<double> result(data.size());

    check_align(&data[0]);
    check_align(&result[0]);

    benchmark([&](){
        baseline(data, ans);
    }, "Baseline Test");

    benchmark([&](){
        compare_sse_single(data, result);
    }, "SSE2 Test");
    check(ans, result);

    benchmark([&](){
        compare_sse_double(data, result);
    }, "SSE2 Packed Test");
    check(ans, result);

    benchmark([&](){
        compare_avx(data, result);
    }, "AVX Test");
    check(ans, result);
    return 0;
}
