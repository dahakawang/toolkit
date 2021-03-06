#include <iostream>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <vector>
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>
#include <emmintrin.h>
#include <immintrin.h>
#include <cuda_runtime.h>
#include <cuda.h>

using namespace tbb::flow;
using namespace tbb;
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

void generate_file(const string& file) {
    file_guard f(fopen(file.c_str(), "wb"));
    if (!f) throw std::runtime_error("failed open file");

    auto data = generate(1 << 27);
    save_double_list(f.get(), data);
}

/**
 * Allocator for aligned data.
 *
 * Modified from the Mallocator from Stephan T. Lavavej.
 * <http://blogs.msdn.com/b/vcblog/archive/2008/08/28/the-mallocator.aspx>
 */
template <typename T, std::size_t Alignment>
class aligned_allocator
{
	public:
 
		// The following will be the same for virtually all allocators.
		typedef T * pointer;
		typedef const T * const_pointer;
		typedef T& reference;
		typedef const T& const_reference;
		typedef T value_type;
		typedef std::size_t size_type;
		typedef ptrdiff_t difference_type;
 
		T * address(T& r) const
		{
			return &r;
		}
 
		const T * address(const T& s) const
		{
			return &s;
		}
 
		std::size_t max_size() const
		{
			// The following has been carefully written to be independent of
			// the definition of size_t and to avoid signed/unsigned warnings.
			return (static_cast<std::size_t>(0) - static_cast<std::size_t>(1)) / sizeof(T);
		}
 
 
		// The following must be the same for all allocators.
		template <typename U>
		struct rebind
		{
			typedef aligned_allocator<U, Alignment> other;
		} ;
 
		bool operator!=(const aligned_allocator& other) const
		{
			return !(*this == other);
		}
 
		void construct(T * const p, const T& t) const
		{
			void * const pv = static_cast<void *>(p);
 
			new (pv) T(t);
		}
 
		void destroy(T * const p) const
		{
			p->~T();
		}
 
		// Returns true if and only if storage allocated from *this
		// can be deallocated from other, and vice versa.
		// Always returns true for stateless allocators.
		bool operator==(const aligned_allocator& other) const
		{
			return true;
		}
 
 
		// Default constructor, copy constructor, rebinding constructor, and destructor.
		// Empty for stateless allocators.
		aligned_allocator() { }
 
		aligned_allocator(const aligned_allocator&) { }
 
		template <typename U> aligned_allocator(const aligned_allocator<U, Alignment>&) { }
 
		~aligned_allocator() { }
 
 
		// The following will be different for each allocator.
		T * allocate(const std::size_t n) const
		{
			// The return value of allocate(0) is unspecified.
			// Mallocator returns NULL in order to avoid depending
			// on malloc(0)'s implementation-defined behavior
			// (the implementation can define malloc(0) to return NULL,
			// in which case the bad_alloc check below would fire).
			// All allocators can return NULL in this case.
			if (n == 0) {
				return NULL;
			}
 
			// All allocators should contain an integer overflow check.
			// The Standardization Committee recommends that std::length_error
			// be thrown in the case of integer overflow.
			if (n > max_size())
			{
				throw std::length_error("aligned_allocator<T>::allocate() - Integer overflow.");
			}
 
			// Mallocator wraps malloc().
			void * const pv = _mm_malloc(n * sizeof(T), Alignment);
 
			// Allocators should throw std::bad_alloc in the case of memory allocation failure.
			if (pv == NULL)
			{
				throw std::bad_alloc();
			}
 
			return static_cast<T *>(pv);
		}
 
		void deallocate(T * const p, const std::size_t n) const
		{
			_mm_free(p);
		}
 
 
		// The following will be the same for all allocators that ignore hints.
		template <typename U>
		T * allocate(const std::size_t n, const U * /* const hint */) const
		{
			return allocate(n);
		}
 
 
		// Allocators are not required to be assignable, so
		// all allocators should have a private unimplemented
		// assignment operator. Note that this will trigger the
		// off-by-default (enabled under /Wall) warning C4626
		// "assignment operator could not be generated because a
		// base class assignment operator is inaccessible" within
		// the STL headers, but that warning is useless.
	private:
		aligned_allocator& operator=(const aligned_allocator&);
};

typedef vector<double, aligned_allocator<double, 32>> DataVector;
typedef DataVector::iterator DataIterator;
typedef tuple<DataIterator, DataIterator, DataIterator> TbbData;
typedef DataIterator::value_type Real;

DataVector load_double_list(FILE* file) {
    size_t size = get_file_size(file);
    if (size % sizeof(double) != 0) throw std::runtime_error("file size inconsistent");
    size_t numDataPts = size / sizeof(double);

    DataVector ret(numDataPts);
    if (fread(&ret[0], sizeof(double), numDataPts, file) != numDataPts) {
        throw std::runtime_error("failed to read");
    }

    return ret;
}

DataVector load_file(const string& file) {
    file_guard f(fopen(file.c_str(), "rb"));
    if (!f) throw std::runtime_error("failed open file");

    return load_double_list(f.get());
}

void check(const DataVector& base, const DataVector& result) {
    if (base.size() != result.size()) throw std::runtime_error("not match");

    for (size_t i = 0; i < base.size(); ++i) {
        if (base[i] != result[i]) {
            throw std::runtime_error("not match [" + to_string(i) + "] = " + to_string(base[i]) + " - " + to_string(result[i]));
        }
    }
}

void zero_out(DataVector& data) {
    std::fill(data.begin(), data.end(), 0);
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

void print_cuda() {
    int count;
    cudaGetDeviceCount(&count);
    for (int i = 0; i < count; ++i) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        printf("GPU [%d] %d.%d\n", i, prop.major, prop.minor);
    }
}


/* ####################################################################
 *
 *
 *   all kinds of sqrt implementations
 *
 *
 * ####################################################################*/

void baseline(DataVector& num, DataVector& out) {
    for (size_t i = 0; i < out.size(); i++) {
        out[i] = sqrt(num[i]);
    }
}

inline void sqrt_sse_single(double* out, double* num) {
    __m128d in = _mm_load_sd(num);
    _mm_store_sd(out, _mm_sqrt_sd(in, in));
}

void compare_sse_single(DataVector& num, DataVector& out) {
    for (size_t i = 0; i < out.size(); i++) {
        sqrt_sse_single(&out[i], &num[i]);
    }
}

inline void sqrt_sse_double(double* out, double* num) {
    __m128d in = _mm_load_pd(num);
    _mm_store_pd(out, _mm_sqrt_pd(in));
}

void compare_sse_double(DataVector& num, DataVector& out) {
    for (size_t i = 0; i < out.size(); i+=2) {
        sqrt_sse_double(&out[i], &num[i]);
    }

    if (out.size() % 2 != 0) out.back() = sqrt(num.back());
}


inline void sqrt_avx(double* out, double* num) {
    __m256d in = _mm256_load_pd(num);
    _mm256_store_pd(out, _mm256_sqrt_pd(in));
}

void compare_avx_iter(DataIterator begin, DataIterator end, DataIterator out) {
    for (; begin + 4 < end; begin += 4, out += 4) {
        sqrt_avx(&*out, &*begin);
    }

    for (; begin < end; ++begin, ++out) *out = sqrt(*begin);
}

void compare_avx(DataVector& num, DataVector& out) {
    compare_avx_iter(num.begin(), num.end(), out.begin());
}

class TbbBody {
public:
    void operator()(TbbData t) {
        compare_avx_iter(get<0>(t), get<1>(t), get<2>(t));
    }

};

void compare_tbb_avx(DataVector& num, DataVector& out) {
    graph g;
    function_node<TbbData> node(g, unlimited, TbbBody());
    static const size_t SIZE = ((2 << 24) / 4) * 4;

    for (size_t pos = 0; pos < num.size(); pos += SIZE) {
        size_t pos_end = (pos + SIZE >= num.size())? num.size() : pos + SIZE;
        node.try_put(make_tuple(num.begin() + pos, num.begin() + pos_end, out.begin() + pos));
    }

    g.wait_for_all();
}

void compare_tbb_para_for_avx(DataVector& num, DataVector& out) {
    parallel_for(blocked_range<size_t>(0, num.size(), 4 * 2<<20), [&](const blocked_range<size_t>& r) {
        compare_avx_iter(num.begin() + r.begin(), num.begin() + r.end(), out.begin() + r.begin());
    });
}

void check_error(cudaError_t error, const char* str = nullptr) {
    if (error != cudaSuccess) {
        std::cout << cudaGetErrorString(error) << " " << (str? str : "")<< std::endl;
        throw std::runtime_error("GPU Error");
    }
}

#define THREAD_PER_BLOCK 1024
__global__ void bulk_sqrt_kernel(Real* data, size_t count) {
    size_t pos = blockIdx.x * blockDim.x + threadIdx.x;
    if (pos >= count) return;
    data[pos] = sqrt(data[pos]);
}

void _compare_cuda(DataVector& num, DataVector& out, size_t pos, size_t count) {
    cudaError_t ret = cudaSuccess;
    Real* d_array = nullptr;
    ret = cudaMalloc(&d_array, count * sizeof(Real));
    check_error(ret, "cudaMalloc");
    ret = cudaMemcpy(d_array, &num[pos], count * sizeof(Real), cudaMemcpyHostToDevice);
    check_error(ret, "cudaMemcpy");

    bulk_sqrt_kernel<<<count / THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(d_array, count);
    check_error(cudaGetLastError(), "launch");

    ret = cudaMemcpy(&out[pos], d_array, count * sizeof(Real), cudaMemcpyDeviceToHost);
    check_error(ret, "copy back");
    ret = cudaFree(d_array);
    check_error(ret, "free");
}

void compare_cuda(DataVector& num, DataVector& out) {
    size_t size = num.size() / 4;
    for (size_t i = 0; i < num.size(); i += size) {
        size_t s = (i + size > num.size()) ? num.size() - i : size;
        _compare_cuda(num, out, i, s);
    }
}

void _compare_cuda_pinned(double* num, double* out, size_t size, size_t count) {
    cudaError_t ret = cudaSuccess;
    Real* d_array = nullptr;
    ret = cudaMalloc(&d_array, size);
    check_error(ret, "cudaMalloc");
    ret = cudaMemcpy(d_array, num, size, cudaMemcpyHostToDevice);
    check_error(ret, "cudaMemcpy");

    bulk_sqrt_kernel<<<count / THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK>>>(d_array, count);
    check_error(cudaGetLastError(), "launch");

    ret = cudaMemcpy(out, d_array, size, cudaMemcpyDeviceToHost);
    check_error(ret, "copy back");
    ret = cudaFree(d_array);
    check_error(ret, "free");
}

void compare_cuda_pinned(double* num, double* out, int count) {
    size_t size = count / 4;
    for (size_t i = 0; i < count; i += size) {
        size_t s = (i + size > count) ? count - i : size;
        _compare_cuda_pinned(&num[i], &out[i], s * sizeof(double), s);
    }
}

void test_cuda_pinned_mem(DataVector& ans, DataVector& data) {
    ans.resize(0);
    cudaError_t ret = cudaSuccess;
    double* ptr = nullptr;
    ret = cudaMallocHost(&ptr, data.size() * sizeof(data[0]));
    check_error(ret, "cudaMallocHost");
    std::copy(data.begin(), data.end(), ptr);

    benchmark([&](){
        compare_cuda_pinned(ptr, ptr, data.size());
    }, "CUDA Pinned Mem Test");
    for (int i = 0; i < ans.size(); ++i) {
        if (ans[i] != ptr[i]) {
            throw std::runtime_error("not match [" + to_string(i) + "] = " + to_string(ans[i]) + " - " + to_string(ptr[i]));
        }
    }
    ret = cudaFreeHost(ptr);
    check_error(ret, "cudaFreeHost");
}


void _compare_cuda_pinned_stream(double* num, double* out, size_t size, size_t count, Real* d_array, cudaStream_t stream) {
    cudaError_t ret = cudaSuccess;
    ret = cudaMemcpyAsync(d_array, num, size, cudaMemcpyHostToDevice, stream);
    check_error(ret, "cudaMemcpy");

    bulk_sqrt_kernel<<<count / THREAD_PER_BLOCK + 1, THREAD_PER_BLOCK, 0, stream>>>(d_array, count);
    check_error(cudaGetLastError(), "launch");

    ret = cudaMemcpyAsync(out, d_array, size, cudaMemcpyDeviceToHost, stream);
    check_error(ret, "copy back");
}

void compare_cuda_pinned_stream(double* num, double* out, int count) {

    size_t size = count / 4;
    vector<Real*> dmem;
    vector<cudaStream_t> streams;
    for (size_t i = 0; i < count; i += size) {
        Real* d_array = nullptr;
        check_error(cudaMalloc(&d_array, size * sizeof(double)), "cudaMalloc");
        dmem.push_back(d_array);

        cudaStream_t stream;
        check_error( cudaStreamCreate(&stream) , "create stream");
        streams.push_back(stream);
    }

    for (size_t i = 0, pos = 0; i < count; i += size, ++pos) {
        size_t s = (i + size > count) ? count - i : size;
        _compare_cuda_pinned_stream(&num[i], &out[i], s * sizeof(double), s, dmem[pos], streams[pos]);
    }

    for(auto s : streams) {
        cudaStreamSynchronize(s);
        cudaStreamDestroy(s);
    }

    for (auto ptr : dmem) {
        check_error(cudaFree(ptr), "cudaFree");
    }
}

void test_cuda_pinned_mem_stream(DataVector& ans, DataVector& data) {
    ans.resize(0);
    cudaError_t ret = cudaSuccess;
    double* ptr = nullptr;
    ret = cudaMallocHost(&ptr, data.size() * sizeof(data[0]));
    check_error(ret, "cudaMallocHost");
    std::copy(data.begin(), data.end(), ptr);

    benchmark([&](){
        compare_cuda_pinned_stream(ptr, ptr, data.size());
    }, "CUDA Pinned Mem With Multiple Stream Test");
    for (int i = 0; i < ans.size(); ++i) {
        if (ans[i] != ptr[i]) {
            throw std::runtime_error("not match [" + to_string(i) + "] = " + to_string(ans[i]) + " - " + to_string(ptr[i]));
        }
    }
    ret = cudaFreeHost(ptr);
    check_error(ret, "cudaFreeHost");
}

int main(int argc, char *argv[]) {
    //generate_file("./list.txt");
    DataVector data = load_file("./list.txt");
    DataVector ans(data.size());
    DataVector result(data.size());

    check_align(&data[0]);
    check_align(&result[0]);

    zero_out(result);
    benchmark([&](){
        baseline(data, ans);
    }, "Baseline Test");

    zero_out(result);
    benchmark([&](){
        compare_sse_single(data, result);
    }, "SSE2 Test");
    check(ans, result);

    zero_out(result);
    benchmark([&](){
        compare_sse_double(data, result);
    }, "SSE2 Packed Test");
    check(ans, result);

    zero_out(result);
    benchmark([&](){
        compare_avx(data, result);
    }, "AVX Test");
    check(ans, result);

    zero_out(result);
    benchmark([&](){
        compare_tbb_avx(data, result);
    }, "TBB AVX Test");
    check(ans, result);

    zero_out(result);
    benchmark([&](){
        compare_tbb_para_for_avx(data, result);
    }, "TBB parallel_for Test");
    check(ans, result);

    zero_out(result);
    benchmark([&](){
        compare_cuda(data, result);
    }, "CUDA Test");
    check(ans, result);

    test_cuda_pinned_mem(ans, data);
    test_cuda_pinned_mem_stream(ans, data);

    return 0;
}
