/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dnn.hpp"

#if defined(FLOOR_COMPUTE) && !defined(FLOOR_COMPUTE_VULKAN) // TODO: fix vulkan compilation

// special case convolution for VGG with RGB input:
// from RGB(A) image -> buffer
template <uint32_t x_dim, uint32_t y_dim, bool with_relu>
floor_inline_always static void convolution_vgg_rgb_f32(const_image_2d<float4> in_img,
														buffer<float> out_img,
														buffer<const float> filters /* width * height * #in-channels * #out-channels */,
														buffer<const float> biases /* #out-channels */,
														const uint32_t out_channel_count) {
	static constexpr const uint32_t in_channel_count { 3 }; // RGB input
	
	// VGG16 expects YX ordered images
	// -> deal with that by flipping X and Y
	const uint2 coord { global_id.y, global_id.x };
	const uint2 dim { global_size.y, global_size.x };
	
	float3 vals[x_dim * y_dim];
	
	// unroll everything so that we get constant offsets in the read
#pragma clang loop unroll_count(y_dim)
	for (uint32_t y = 0; y < y_dim; ++y) {
#pragma clang loop unroll_count(x_dim)
		for (uint32_t x = 0; x < x_dim; ++x) {
			float3 bgr; // zero-init
			const int2 read_coord = coord.cast<int>() + int2 { int(x) - int(x_dim / 2), int(y) - int(y_dim / 2) };
			if ((read_coord < 0).any() || (read_coord >= dim).any()) {
				// pad with 0
			} else {
				// scale to [0, 255], swizzle to BGR, substract mean
				// NOTE: this is special for VGG16 (TODO: do this in a general way - for now: keep for perf reasons)
				bgr = (in_img.read(coord,
								   int2 {
									   int(x) - int(x_dim / 2),
									   int(y) - int(y_dim / 2)
								   }).xyz * 255.0f).swizzle<2, 1, 0>() - float3 { 103.939f, 116.779f, 123.68f };
			}
			
			// flip here as well
			vals[y + x * y_dim] = bgr;
		}
	}
	
	// per-out-channel convolution in WHIO order
	for (uint32_t out_channel = 0; out_channel < out_channel_count; ++out_channel) {
		float conv_sum = 0.0f;
#pragma clang loop unroll_count(in_channel_count)
		for (uint32_t in_channel = 0; in_channel < in_channel_count; ++in_channel) {
#pragma clang loop unroll_count(y_dim)
			for (uint32_t y = 0; y < y_dim; ++y) {
#pragma clang loop unroll_count(x_dim)
				for (uint32_t x = 0; x < x_dim; ++x) {
					conv_sum += vals[x + y * x_dim][in_channel] * filters[x * y_dim * in_channel_count * out_channel_count +
																		  y * in_channel_count * out_channel_count +
																		  in_channel * out_channel_count +
																		  out_channel];
				}
			}
		}
		
		// bias
		conv_sum += biases[out_channel];
		
		// relu (if specified)
		const auto out_idx = global_id.x * global_size.y * out_channel_count + global_id.y * out_channel_count + out_channel; // WHC order
		if constexpr (with_relu) {
			out_img[out_idx] = max(conv_sum, 0.0f);
		} else {
			out_img[out_idx] = conv_sum;
		}
	}
}

kernel void convolution_vgg_rgb_3x3_relu_f32(const_image_2d<float4> in_img,
											 buffer<float> out_img,
											 buffer<const float> filters,
											 buffer<const float> biases,
											 param<uint32_t> out_channel_count) {
	convolution_vgg_rgb_f32<3, 3, true>(in_img, out_img, filters, biases, out_channel_count);
}

// general convolution (from buffer -> buffer):
// NOTE: with 2D image dim == group size (1 group per pixel)
// NOTE: in_channel == local_id.x
template <uint32_t x_dim, uint32_t y_dim, uint32_t in_channel_count, bool with_relu>
floor_inline_always static void convolution_f32(buffer<const float> in_data,
												buffer<float> out_data,
												buffer<const float> filters /* width * height * #in-channels * #out-channels */,
												buffer<const float> biases /* #out-channels */,
												const uint32_t out_channel_count) {
	const uint2 coord = group_id.xy;
	const uint2 dim = group_size.xy;
	const uint32_t out_channel = local_id.x;
	
	// cache current (x, y) input layer data
	// -> stored in a ping pong fashion so that we only need only barrier
	local_buffer<float, in_channel_count * 2> in_channel_data;
	uint32_t ping_pong = 0;
	
	float conv_sum = 0.0f;
#pragma nounroll
	for (uint32_t x = 0; x < x_dim; ++x) {
#pragma nounroll
		for (uint32_t y = 0; y < y_dim; ++y) {
			const int2 read_coord = coord.cast<int>() + int2 { int(x) - int(x_dim / 2), int(y) - int(y_dim / 2) };
			if ((read_coord < 0).any() || (read_coord >= dim).any()) {
				// note that this is uniformly executed for all work-items in a group
				continue;
			}
			
			// cache data for this pixel position in all input channels
			if (local_id.x < in_channel_count) {
				const auto read_coord_u32 = read_coord.cast<uint32_t>();
				in_channel_data[local_id.x + ping_pong] = in_data[read_coord_u32.x * in_channel_count * group_size.y + read_coord_u32.y * in_channel_count + local_id.x]; // WHC order
			}
			local_barrier(); // done writing to this ping pong region
			
#pragma clang loop unroll_count(8)
			for (uint32_t in_channel = 0; in_channel < in_channel_count; ++in_channel) {
				conv_sum += in_channel_data[in_channel + ping_pong] * filters[x * y_dim * in_channel_count * out_channel_count +
																			  y * in_channel_count * out_channel_count +
																			  in_channel * out_channel_count +
																			  out_channel]; // WHIO order
			}
			
			// shift range for next loop
			ping_pong = in_channel_count - ping_pong;
		}
	}

	// bias
	conv_sum += biases[out_channel];
	
	// relu (if specified)
	const auto out_idx = group_id.x * group_size.y * out_channel_count + group_id.y * out_channel_count + out_channel; // WHC order
	if constexpr (with_relu) {
		out_data[out_idx] = max(conv_sum, 0.0f);
	} else {
		out_data[out_idx] = conv_sum;
	}
}

// TODO: specialize for out_channel_count as well?
#define CONV_KERNELS(in_channel_count) \
kernel /*kernel_local_size(in_channel_count, 1, 1)*/ /* TODO: should be out_channel_count */ \
void convolution_3x3_ ## in_channel_count ## _relu_f32(buffer<const float> in_data, \
													   buffer<float> out_data, \
													   buffer<const float> filters, \
													   buffer<const float> biases, \
													   param<uint32_t> out_channel_count) { \
	convolution_f32<3, 3, in_channel_count, true>(in_data, out_data, filters, biases, out_channel_count); \
}

CONV_KERNELS(64)
CONV_KERNELS(128)
CONV_KERNELS(256)
CONV_KERNELS(512)

// <width, height> -> <width / 2, height / 2>, with max(quad samples)
// global_size.x: (width / 2) * (height / 2) * #layers
// run with 1D-linearized output shape
// NOTE: expecting WHC-ordered data and width == height
kernel void max_pooling(buffer<const float> in_data,
						buffer<float> out_data,
						// input (width, height, channels) -> output (width / 2, height / 2, channels)
						param<uint3> in_shape) {
	const uint2 out_xy_shape { in_shape.x >> 1u, in_shape.y >> 1u };
	
	// WHC coord
	uint3 coord {
		global_id.x / (in_shape.z * out_xy_shape.y),
		(global_id.x / in_shape.z) % out_xy_shape.y,
		global_id.x % in_shape.z,
	};
	coord.xy *= 2u;
	
	const float vals[] {
		in_data[coord.x * in_shape.y * in_shape.z + coord.y * in_shape.z + coord.z],
		in_data[coord.x * in_shape.y * in_shape.z + (coord.y + 1u) * in_shape.z + coord.z],
		in_data[(coord.x + 1u) * in_shape.y * in_shape.z + coord.y * in_shape.z + coord.z],
		in_data[(coord.x + 1u) * in_shape.y * in_shape.z + (coord.y + 1u) * in_shape.z + coord.z],
	};
	
	out_data[global_id.x] = max(max(max(vals[0], vals[1]), vals[2]), vals[3]);
}

// sum reduction using either sub-groups or local memory reduction
template <uint32_t tile_size, typename data_type>
floor_inline_always static data_type sum_reduction(const data_type& initial_val) {
	static constexpr const uint32_t sub_group_width { max(1u, device_info::simd_width_min()) };
	if constexpr (device_info::has_sub_group_shuffle() && sub_group_width == 32u) { // TODO: support sub_group_width != 32
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
		static constexpr const uint32_t tile_count { max(1u, tile_size / sub_group_width) };
		static_assert(tile_count * sub_group_width == tile_size, "tile size must be a multiple of sub-group width");
		
		// reduce in sub-group first (via shuffle)
		const auto partial_sum = compute_algorithm::sub_group_reduce_add(initial_val);
		
		// write reduced sub-group value to local memory for the next step
		local_buffer<data_type, tile_count> lmem;
		if (sub_group_local_id == 0) {
			lmem[sub_group_id_1d] = partial_sum;
		}
		local_barrier();
		
		// final reduction of sub-group values
		// -> can do this directly in one sub-group
		if constexpr (tile_size > sub_group_width) {
			if (sub_group_id_1d == 0) {
				const auto final_red_sum = compute_algorithm::sub_group_reduce_add(sub_group_local_id < tile_count ?
																				   lmem[sub_group_local_id] : 0.0f);
				if (sub_group_local_id == 0) {
					lmem[0] = final_red_sum;
				}
			}
			local_barrier();
		}
		
		const auto ret_sum = lmem[0];
		local_barrier(); // again, because we might use local memory again
		
		return ret_sum;
#endif
	} else {
		// local memory reduction
		
		// local reduction to work-item #0
		local_buffer<data_type, compute_algorithm::reduce_local_memory_elements<tile_size>()> lmem;
		const auto red_sum = compute_algorithm::reduce<tile_size>(initial_val, lmem, plus<> {});
		
		// only work-item #0 knows the reduced sum -> broadcast via local memory
		if (local_id.x == 0u) {
			lmem[0] = red_sum;
		}
		local_barrier();
		
		const auto ret_sum = lmem[0];
		local_barrier(); // again, because we might use local memory again
		
		return ret_sum;
	}
}

// matrix multiplication for the fully-connected part of the network,
// first step that computes the partial sums of the output matrix
// A: height = 1,       width = A_width
// B: height = A_width, width = B_width
// C: height = 1,       width = B_width
template <typename in_data_type, typename out_data_type, uint32_t tile_size, bool unchecked>
floor_inline_always static void fc_matrix_mul_partial(buffer<const in_data_type> A,
													  buffer<const out_data_type> B,
													  buffer<out_data_type> C_partials,
													  const uint32_t A_width,
													  const uint32_t B_width,
													  const uint32_t partial_count) {
	const uint32_t elem_idx = global_id.x;
	
	float a_elem;
	if constexpr (unchecked) {
		a_elem = float(A[elem_idx]);
	} else {
		a_elem = (elem_idx < A_width ? float(A[elem_idx]) : 0.0f);
	}
	
	// we know that B_width is always a multiple of 8
	// TODO: figure out why the partial unroll doesn't work
//#pragma clang loop unroll_count(8)
	for (uint32_t i = 0; i < B_width; ++i) {
		float b_elem;
		if constexpr (unchecked) {
			b_elem = float(B[B_width * elem_idx + i]); // row-wise
		} else {
			b_elem = (elem_idx < A_width ? float(B[B_width * elem_idx + i]) : 0.0f); // row-wise
		}
		
		// elem sum
		const auto c_elem = a_elem * b_elem;
		
		// sum reduction for final partial sum
		const auto c_partial_sum = sum_reduction<tile_size>(c_elem);
		
		// only one work-item needs to write this
		if (local_id.x == 0) {
			// TODO: figure out the best mem order for this (also for later read in total sum computation)
			// -> store partials sequentially
			//C_partials[group_id.x * B_width + i] = c_partial_sum; // row-wise
			C_partials[i * partial_count /* group_size.x */ + group_id.x] = out_data_type(c_partial_sum); // column-wise
		}
	}
}

// matrix multiplication for the fully-connected part of the network,
// second step that computes the total sums of the output matrix
// A: height = 1,       width = A_width
// B: height = A_width, width = B_width
// C: height = 1,       width = B_width
template <typename data_type>
floor_inline_always static void fc_matrix_mul_total(buffer<const data_type> C_partials,
													buffer<data_type> C,
													buffer<const data_type> biases,
													param<uint32_t> partial_count,
													param<uint32_t> B_width,
													param<uint32_t> with_relu) {
	if (global_id.x >= B_width) {
		return;
	}
	
	// NOTE: we assume that partials are stored sequentially
	const auto offset = global_id.x * partial_count;
	float total_sum = 0.0f;
	for (uint32_t i = 0; i < partial_count; ++i) {
		total_sum += float(C_partials[offset + i]);
	}
	total_sum += float(biases[global_id.x]);
	C[global_id.x] = data_type(with_relu ? max(total_sum, 0.0f) : total_sum);
}

kernel void fc_matrix_mul_total_f32(buffer<const float> C_partials,
									buffer<float> C,
									buffer<const float> biases,
									param<uint32_t> partial_count,
									param<uint32_t> B_width,
									param<uint32_t> with_relu) {
	fc_matrix_mul_total<float>(C_partials, C, biases, partial_count, B_width, with_relu);
}

kernel void fc_matrix_mul_total_f16(buffer<const half> C_partials,
									buffer<half> C,
									buffer<const half> biases,
									param<uint32_t> partial_count,
									param<uint32_t> B_width,
									param<uint32_t> with_relu) {
	fc_matrix_mul_total<half>(C_partials, C, biases, partial_count, B_width, with_relu);
}

// instantiate for different tile sizes
#define FC_MATRIX_MUL_KERNEL(tile_size, in_data_type, in_data_type_name, out_data_type, out_data_type_name) \
kernel kernel_local_size(tile_size, 1, 1) void fc_matrix_mul_partial_checked_ ## tile_size ## _ ## in_data_type_name ## _ ## out_data_type_name (buffer<const in_data_type> A, \
																																				 buffer<const out_data_type> B, \
																																				 buffer<out_data_type> C_partials, \
																																				 param<uint32_t> A_width, \
																																				 param<uint32_t> B_width, \
																																				 param<uint32_t> partial_count) { \
	fc_matrix_mul_partial<in_data_type, out_data_type, tile_size, false>(A, B, C_partials, A_width, B_width, partial_count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void fc_matrix_mul_partial_unchecked_ ## tile_size ## _ ## in_data_type_name ## _ ## out_data_type_name (buffer<const in_data_type> A, \
																																				   buffer<const out_data_type> B, \
																																				   buffer<out_data_type> C_partials, \
																																				   param<uint32_t> A_width, \
																																				   param<uint32_t> B_width, \
																																				   param<uint32_t> partial_count) { \
	fc_matrix_mul_partial<in_data_type, out_data_type, tile_size, true>(A, B, C_partials, A_width, B_width, partial_count); \
}

FC_MATRIX_MUL_KERNEL(1024, float, f32, float, f32)
FC_MATRIX_MUL_KERNEL(512, float, f32, float, f32)
FC_MATRIX_MUL_KERNEL(256, float, f32, float, f32)
FC_MATRIX_MUL_KERNEL(128, float, f32, float, f32)
FC_MATRIX_MUL_KERNEL(64, float, f32, float, f32)
FC_MATRIX_MUL_KERNEL(32, float, f32, float, f32)
FC_MATRIX_MUL_KERNEL(1024, float, f32, half, f16)
FC_MATRIX_MUL_KERNEL(512, float, f32, half, f16)
FC_MATRIX_MUL_KERNEL(256, float, f32, half, f16)
FC_MATRIX_MUL_KERNEL(128, float, f32, half, f16)
FC_MATRIX_MUL_KERNEL(64, float, f32, half, f16)
FC_MATRIX_MUL_KERNEL(32, float, f32, half, f16)
FC_MATRIX_MUL_KERNEL(1024, half, f16, float, f32)
FC_MATRIX_MUL_KERNEL(512, half, f16, float, f32)
FC_MATRIX_MUL_KERNEL(256, half, f16, float, f32)
FC_MATRIX_MUL_KERNEL(128, half, f16, float, f32)
FC_MATRIX_MUL_KERNEL(64, half, f16, float, f32)
FC_MATRIX_MUL_KERNEL(32, half, f16, float, f32)
FC_MATRIX_MUL_KERNEL(1024, half, f16, half, f16)
FC_MATRIX_MUL_KERNEL(512, half, f16, half, f16)
FC_MATRIX_MUL_KERNEL(256, half, f16, half, f16)
FC_MATRIX_MUL_KERNEL(128, half, f16, half, f16)
FC_MATRIX_MUL_KERNEL(64, half, f16, half, f16)
FC_MATRIX_MUL_KERNEL(32, half, f16, half, f16)

// single-pass softmax with sub-group reduce:
// usable if #elems <= max local size, and if max local size >= tile_size
// NOTE: must always be called with a local size of "tile_size"
template <typename in_data_type, typename out_data_type, uint32_t tile_size>
floor_inline_always static void softmax_inplace(buffer<const in_data_type> in_data, buffer<out_data_type> out_data, param<uint32_t> count) {
	if (count > tile_size) {
		float sum = 0.0f;
		for (uint32_t i = local_id.x; i < count; i += tile_size) {
			// initial exp(item_i) val (or 0 if oob)
			sum += exp(float(in_data[i]));
		}
		
		// sum reduction
		const auto inv_sum = 1.0f / sum_reduction<tile_size>(sum);
		local_barrier();
		
		// do the soft max
		for (uint32_t i = local_id.x; i < count; i += tile_size) {
			out_data[i] = out_data_type(exp(float(in_data[i])) * inv_sum);
		}
	} else { // direct computation
		const auto val = (local_id.x < count ? exp(float(in_data[local_id.x])) : 0.0f);
		const auto inv_sum = 1.0f / sum_reduction<tile_size>(val);
		if (local_id.x < count) {
			out_data[local_id.x] = out_data_type(val * inv_sum);
		}
	}
}

#define SOFTMAX_INPLACE_KERNEL(tile_size, in_data_type, in_data_type_name, out_data_type, out_data_type_name) \
kernel kernel_local_size(tile_size, 1, 1) void softmax_inplace_ ## tile_size ## _ ## in_data_type_name ## _ ## out_data_type_name (buffer<const in_data_type> in_data, \
																																   buffer<out_data_type> out_data, \
																																   param<uint32_t> count) { \
	softmax_inplace<in_data_type, out_data_type, tile_size>(in_data, out_data, count); \
}

SOFTMAX_INPLACE_KERNEL(1024, float, f32, float, f32)
SOFTMAX_INPLACE_KERNEL(512, float, f32, float, f32)
SOFTMAX_INPLACE_KERNEL(256, float, f32, float, f32)

// for now: always 32-bit float output
SOFTMAX_INPLACE_KERNEL(1024, half, f16, float, f32)
SOFTMAX_INPLACE_KERNEL(512, half, f16, float, f32)
SOFTMAX_INPLACE_KERNEL(256, half, f16, float, f32)

// multi-pass softmax:
// 1st: compute exp(item_i)
kernel void exp_inplace(buffer<float> data) {
	data[global_id.x] = exp(data[global_id.x]);
}

// 2nd: reduction to compute the sum
// NOTE: we make use of the common reduction kernels here (-> common/reduction.cpp)
// TODO: implement this

// 3rd: read back sum on cpu -> inv_sum = 1 / sum -> do the final softmax computation
kernel void softmax(buffer<float> data, param<float> inv_sum) {
	data[global_id.x] = data[global_id.x] * inv_sum;
}

// resamples a larger image to (224, 224)
kernel void resample_image_224(const_image_2d<float4> in_img, image_2d<float4, true> out_img, param<uint2> in_dim) {
	static constexpr const uint2 out_dim { 224, 224 };
	if (global_id.x >= out_dim.x || global_id.y >= out_dim.y) return;
	
	const uint2 samples_per_pixel = (in_dim / out_dim).max(1u);
	float4 sum;
	for (uint32_t y = 0; y < samples_per_pixel.y; ++y) {
		for (uint32_t x = 0; x < samples_per_pixel.x; ++x) {
			sum += in_img.read(global_id.xy * samples_per_pixel, int2 { int(x), int(y) });
		}
	}
	sum /= float(samples_per_pixel.x * samples_per_pixel.y);
	out_img.write(global_id.xy, sum);
}

#endif
