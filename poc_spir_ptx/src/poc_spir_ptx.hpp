
#ifndef __POC_SPIR_PTX_HPP__
#define __POC_SPIR_PTX_HPP__

#include <floor/math/vector_lib.hpp>

// opencl/cuda syntax differ slightly from std c++ syntax in that they require certain
// keywords in certain places, which the host compiler doesn't understand.
// -> add a cheap workaround for now
// (note that proper source compat is still wip)
#if !defined(FLOOR_COMPUTE)
#include <floor/core/cpp_headers.hpp>
#define kernel
#define global
#define constant
#define opencl_constant
#define device_constant
static atomic<uint32_t> global_id_counter { 0 };
template <class data_type, size_t array_size> using const_array = data_type[array_size];
#define get_global_id(dim) (global_id_counter++)
void reset_counter();

namespace floor_compute {
	template <typename T> struct indirect_type_wrapper {
		T elem;
		
		indirect_type_wrapper(const T& obj) : elem(obj) {}
		indirect_type_wrapper& operator=(const T& obj) {
			elem = obj;
			return *this;
		}
		const T& operator*() const { return elem; }
		T& operator*() { return elem; }
		const T* const operator->() const { return &elem; }
		T* operator->() { return &elem; }
		operator T() const { return elem; }
	};
	template <typename T> struct direct_type_wrapper : T {
		using T::T;
		direct_type_wrapper& operator=(const T& obj) {
			*((T*)this) = obj;
			return *this;
		}
		const T& operator*() const { return *this; }
		T& operator*() { return *this; }
		const T* const operator->() const { return this; }
		T* operator->() { return this; }
	};
}
template <typename T,
		  typename param_wrapper = conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::indirect_type_wrapper<T>,
			  floor_compute::direct_type_wrapper<T>>>
using buffer = param_wrapper*;
template <typename T> using local_buffer = floor_compute::indirect_type_wrapper<T>*;
template <typename T> using const_buffer = const floor_compute::indirect_type_wrapper<T>* const;
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::indirect_type_wrapper<T>,
			  floor_compute::direct_type_wrapper<T>>>
using param = const param_wrapper;
#else
#define reset_counter()
#endif

// prototypes
kernel void path_trace(buffer<float3> img,
					   param<uint32_t> iteration,
					   param<uint32_t> seed,
					   param<uint2> img_size);

#endif
