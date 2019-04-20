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

#include "nn_executer.hpp"
#include <floor/compute/compute_kernel.hpp>

//// program/kernels are only loaded/initialized once
static shared_ptr<compute_program> dnn_prog;
// FC combinations
#define FC_MATRIX_MUL_KERNEL(F, tile_size, in_data_type, in_data_type_name, out_data_type, out_data_type_name) \
F(fc_matrix_mul_partial_checked_ ## tile_size ## _ ## in_data_type_name ## _ ## out_data_type_name) \
F(fc_matrix_mul_partial_unchecked_ ## tile_size ## _ ## in_data_type_name ## _ ## out_data_type_name)

// softmax combinations
#define SOFTMAX_INPLACE_KERNEL(F, tile_size, in_data_type, in_data_type_name, out_data_type, out_data_type_name) \
F(softmax_inplace_ ## tile_size ## _ ## in_data_type_name ## _ ## out_data_type_name)

// all kernels
#define DNN_KERNELS(F) \
F(resample_image_224) \
F(convolution_vgg_rgb_3x3_relu_f32) \
F(convolution_3x3_64_relu_f32) \
F(convolution_3x3_128_relu_f32) \
F(convolution_3x3_256_relu_f32) \
F(convolution_3x3_512_relu_f32) \
F(max_pooling) \
F(exp_inplace) \
F(softmax) \
/* softmax in can be f16/f32, output is always f32 */ \
SOFTMAX_INPLACE_KERNEL(F, 1024, float, f32, float, f32) \
SOFTMAX_INPLACE_KERNEL(F, 512, float, f32, float, f32) \
SOFTMAX_INPLACE_KERNEL(F, 256, float, f32, float, f32) \
SOFTMAX_INPLACE_KERNEL(F, 1024, half, f16, float, f32) \
SOFTMAX_INPLACE_KERNEL(F, 512, half, f16, float, f32) \
SOFTMAX_INPLACE_KERNEL(F, 256, half, f16, float, f32) \
F(fc_matrix_mul_total_f32) \
F(fc_matrix_mul_total_f16) \
/* FC in/out can be f16/f32 */ \
FC_MATRIX_MUL_KERNEL(F, 1024, float, f32, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 512, float, f32, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 256, float, f32, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 128, float, f32, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 64, float, f32, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 32, float, f32, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 1024, float, f32, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 512, float, f32, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 256, float, f32, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 128, float, f32, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 64, float, f32, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 32, float, f32, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 1024, half, f16, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 512, half, f16, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 256, half, f16, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 128, half, f16, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 64, half, f16, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 32, half, f16, float, f32) \
FC_MATRIX_MUL_KERNEL(F, 1024, half, f16, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 512, half, f16, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 256, half, f16, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 128, half, f16, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 64, half, f16, half, f16) \
FC_MATRIX_MUL_KERNEL(F, 32, half, f16, half, f16)

//
#define DNN_KERNEL_ENUM(func) DNN_ ## func,
enum DNN_KERNEL : uint32_t {
	DNN_KERNELS(DNN_KERNEL_ENUM)
	__MAX_DNN_KERNEL
};
#undef DNN_KERNEL_ENUM
//
#define DNN_KERNEL_STR(func) #func,
static const array<const char*, __MAX_DNN_KERNEL> dnn_kernel_names {{
	DNN_KERNELS(DNN_KERNEL_STR)
}};
#undef DNN_KERNEL_STR
//
static array<shared_ptr<compute_kernel>, __MAX_DNN_KERNEL> dnn_kernels;
//
static array<uint1, __MAX_DNN_KERNEL> dnn_max_local_sizes;
static array<uint3, __MAX_DNN_KERNEL> dnn_max_local_sizes_3d;

bool nn_executer::init_kernels() {
	return rebuild_kernels();
}

bool nn_executer::rebuild_kernels() {
#if !defined(FLOOR_IOS)
	llvm_toolchain::compile_options options {
		.cli = ("-I" + floor::data_path("../dnn/src")),
		.cuda.ptx_version = 60,
	};
	auto new_dnn_prog = dnn_state.ctx->add_program_file(floor::data_path("../dnn/src/dnn.cpp"), options);
#else
	auto new_dnn_prog = dnn_state.ctx->add_universal_binary(floor::data_path("dnn.fubar"));
#endif
	if(new_dnn_prog == nullptr) {
		log_error("program compilation failed");
		return false;
	}
	
	decltype(dnn_kernels) new_dnn_kernels;
	decltype(dnn_max_local_sizes) new_dnn_max_local_sizes;
	decltype(dnn_max_local_sizes_3d) new_dnn_max_local_sizes_3d;
	for(uint32_t i = 0; i < DNN_KERNEL::__MAX_DNN_KERNEL; ++i) {
		new_dnn_kernels[i] = new_dnn_prog->get_kernel(dnn_kernel_names[i]);
		if (new_dnn_kernels[i] == nullptr && string(dnn_kernel_names[i]).find("1024") != string::npos) {
			new_dnn_max_local_sizes[i] = 0;
			new_dnn_max_local_sizes_3d[i] = { 0, 0, 0 };
			log_debug("%s: unavailable on this device", dnn_kernel_names[i]);
			continue;
		}
		if (new_dnn_kernels[i] == nullptr) {
			log_error("failed to retrieve kernel %s from program", dnn_kernel_names[i]);
			return false;
		}
		new_dnn_max_local_sizes[i] = new_dnn_kernels[i]->get_kernel_entry(*dnn_state.dev)->max_total_local_size;
		new_dnn_max_local_sizes_3d[i] = new_dnn_kernels[i]->get_kernel_entry(*dnn_state.dev)->max_local_size;
		log_debug("%s: local size: %u", dnn_kernel_names[i], new_dnn_max_local_sizes[i]);
	}
	
	// everything was successful, exchange objects
	dnn_prog = new_dnn_prog;
	dnn_kernels = new_dnn_kernels;
	dnn_max_local_sizes = new_dnn_max_local_sizes;
	dnn_max_local_sizes_3d = new_dnn_max_local_sizes_3d;
	return true;
}

nn_executer::nn_executer(const nn_model& mdl_, const compute_device& dev_, shared_ptr<compute_queue> dev_queue_) :
mdl(mdl_), dev(dev_), dev_queue(dev_queue_) {
}

void nn_executer::dump(const string name, const uint64_t max_elems) {
	auto input = get_if<const compute_buffer*>(&cur_input);
	if (input == nullptr) {
		return;
	}
	auto data_buf = const_cast<compute_buffer*>(*input);
	
	const uint64_t elem_count = (data_buf->get_size() / sizeof(float));
	if (elem_count == 0 || max_elems == 0) return;
	const uint64_t display_elem_count = (max_elems != ~0ull ? min(elem_count, max_elems) : elem_count);
	
	auto data = (float*)data_buf->map(*dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ);
	auto data_ptr = data;
	
	stringstream data_sstr;
	data_sstr << fixed << scientific;
	data_sstr.precision(8);
	for (uint64_t i = 0; i < display_elem_count; ++i) {
		data_sstr << *data_ptr++ << ", ";
	}
	log_undecorated("%s data:\n%s", name, data_sstr.str());
	
	data_buf->unmap(*dev_queue, data);
}

shared_ptr<compute_image> nn_executer::resample(shared_ptr<compute_image> img) {
	auto ret = dnn_state.ctx->create_image(*dnn_state.dev_queue, uint4 { 224, 224, 1, 0 }, img->get_image_type());
	const uint2 in_dim = img->get_image_dim().xy;
	dev_queue->execute(dnn_kernels[DNN_resample_image_224],
					   uint2 { 224, 224 },
					   // TODO: find a proper supported size
					   uint2 { 16, 16 },
					   img, ret, in_dim);
	dev_queue->finish();
	return ret;
}

void nn_executer::clear() {
	input_img = nullptr;
	input_buf = nullptr;
	stage_input = nullptr;
	stage_output = nullptr;
	cur_input = decltype(cur_input)();
	cur_dim = 0;
	cur_data_type = DATA_TYPE::FLOAT_32;
}

void nn_executer::input(const compute_image& img, const bool rgba_as_rgb) {
	input_img = &img;
	cur_input = input_img;
	
	input_dim.xy = img.get_image_dim().xy;
	input_dim.z = image_channel_count(img.get_image_type());
	if (input_dim.z == 4 && rgba_as_rgb) {
		input_dim.z = 3;
	}
	cur_dim = input_dim;
}

void nn_executer::input(const compute_buffer& buf, const uint3 dim) {
	input_buf = &buf;
	cur_input = input_buf;
	cur_dim = dim;
}

void nn_executer::convolution(const string& layer_name, const bool dump_output) {
	const auto layer = mdl.get_conv_layer(layer_name);
	if (!layer) {
		log_error("no such layer: %s", layer_name);
		return;
	}
	
	if (cur_data_type != DATA_TYPE::FLOAT_32) {
		log_error("%s: data type != float is currently not supported", layer_name);
		return;
	}
	
	const auto& conv = **layer;
	const auto conv_out_channel_count = conv.get_output_channels();
	const auto conv_out_elem_count = cur_dim.xy.extent() * conv_out_channel_count;
	stage_output = dnn_state.ctx->create_buffer(*dev_queue, sizeof(float) * conv_out_elem_count,
												COMPUTE_MEMORY_FLAG::READ_WRITE | (dump_output ?
																				   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE :
																				   COMPUTE_MEMORY_FLAG::NONE));
	
	if (auto img = get_if<const compute_image*>(&cur_input)) {
		if (cur_dim.z != 3) {
			log_error("layer %s: non-RGB image convolution not implemented", layer_name);
			return;
		}
		const uint2 conv_local_dim { dnn_max_local_sizes_3d[DNN_convolution_vgg_rgb_3x3_relu_f32].xy };
		log_debug("running conv kernel #%u: global: %v, local %v",
				  DNN_convolution_vgg_rgb_3x3_relu_f32, cur_dim.xy, conv_local_dim);
		dev_queue->execute(dnn_kernels[DNN_convolution_vgg_rgb_3x3_relu_f32],
						   cur_dim.xy,
						   // TODO: find a proper supported size
						   //conv_local_dim,
						   uint2 { 16, 16 },
						   *img, stage_output, conv.filters, conv.biases, conv_out_channel_count);
	} else if (auto buf = get_if<const compute_buffer*>(&cur_input)) {
		auto kernel_idx = DNN_convolution_3x3_64_relu_f32;
		switch (cur_dim.z) {
			case 64: kernel_idx = DNN_convolution_3x3_64_relu_f32; break;
			case 128: kernel_idx = DNN_convolution_3x3_128_relu_f32; break;
			case 256: kernel_idx = DNN_convolution_3x3_256_relu_f32; break;
			case 512: kernel_idx = DNN_convolution_3x3_512_relu_f32; break;
			default:
				log_error("layer %s: convolution with an input layer count of %u is not implemented", layer_name, cur_dim.z);
				return;
		}
		log_debug("running conv kernel #%u: global: %v, local %v, #out-ch: %u",
				  kernel_idx, uint2 { cur_dim.x * conv_out_channel_count, cur_dim.y }, uint2 { conv_out_channel_count, 1 }, conv_out_channel_count);
		dev_queue->execute(dnn_kernels[kernel_idx],
						   uint2 { cur_dim.x * conv_out_channel_count, cur_dim.y },
						   uint2 { conv_out_channel_count, 1 },
						   *buf, stage_output, conv.filters, conv.biases, conv_out_channel_count);
	} else {
		log_error("layer %s: no input set", layer_name);
		return;
	}
	
	dev_queue->finish();
	
	// update for next stage
	stage_input = stage_output;
	stage_output = nullptr;
	cur_input = stage_input.get();
	cur_dim.z = conv_out_channel_count;
	
	if (dump_output) {
		dump(layer_name);
	}
}

void nn_executer::max_pooling(const bool dump_output) {
	auto input = get_if<const compute_buffer*>(&cur_input);
	if (input == nullptr) {
		log_error("no input buffer set or unsupported image input");
		return;
	}
	
	if (cur_data_type != DATA_TYPE::FLOAT_32) {
		log_error("data type != float is currently not supported");
		return;
	}
	
	const uint2 max_pool_dim { cur_dim.x / 2, cur_dim.y / 2 };
	const auto max_pool_out_elem_count = max_pool_dim.extent() * cur_dim.z;
	stage_output = dnn_state.ctx->create_buffer(*dev_queue, sizeof(float) * max_pool_out_elem_count,
												COMPUTE_MEMORY_FLAG::READ_WRITE | (dump_output ?
																				   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE :
																				   COMPUTE_MEMORY_FLAG::NONE));
	
	log_debug("running max pooling (%u)", max_pool_out_elem_count);
	dev_queue->execute(dnn_kernels[DNN_max_pooling],
					   uint1 { max_pool_out_elem_count },
					   uint1 { dnn_max_local_sizes[DNN_max_pooling] },
					   *input, stage_output, cur_dim);
	dev_queue->finish();
	
	// update for next stage
	stage_input = stage_output;
	stage_output = nullptr;
	cur_input = stage_input.get();
	cur_dim.xy = max_pool_dim;
	
	if (dump_output) {
		dump("max pooling");
	}
}

void nn_executer::fully_connected(const string& layer_name, const bool with_relu, const bool dump_output) {
	const auto layer = mdl.get_fully_connected_layer(layer_name);
	if (!layer) {
		log_error("no such layer: %s", layer_name);
		return;
	}
	
	auto input = get_if<const compute_buffer*>(&cur_input);
	if (input == nullptr) {
		log_error("no input buffer set or unsupported image input");
		return;
	}
	
	const auto& fc = **layer;
	const auto fc_width = fc.get_width();
	const auto fc_height = fc.get_height();
	const auto fc_out_elem_count = fc_width;
	
	const auto in_type = cur_data_type;
	const auto out_type = fc.type;
	const auto out_type_size = data_type_size(out_type);
	
	stage_output = dnn_state.ctx->create_buffer(*dev_queue, out_type_size * fc_out_elem_count,
												COMPUTE_MEMORY_FLAG::READ_WRITE | (dump_output ?
																				   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE :
																				   COMPUTE_MEMORY_FLAG::NONE));
	
	uint32_t local_size = 1024;
	uint32_t kernel_idx = 0, kernel_end_idx = 0;
	if (in_type == DATA_TYPE::FLOAT_32 && out_type == DATA_TYPE::FLOAT_32) {
		kernel_idx = DNN_fc_matrix_mul_partial_checked_1024_f32_f32;
		kernel_end_idx = DNN_fc_matrix_mul_partial_checked_32_f32_f32 + 2;
	} else if (in_type == DATA_TYPE::FLOAT_16 && out_type == DATA_TYPE::FLOAT_32) {
		kernel_idx = DNN_fc_matrix_mul_partial_checked_1024_f16_f32;
		kernel_end_idx = DNN_fc_matrix_mul_partial_checked_32_f16_f32 + 2;
	} else if (in_type == DATA_TYPE::FLOAT_32 && out_type == DATA_TYPE::FLOAT_16) {
		kernel_idx = DNN_fc_matrix_mul_partial_checked_1024_f32_f16;
		kernel_end_idx = DNN_fc_matrix_mul_partial_checked_32_f32_f16 + 2;
	} else if (in_type == DATA_TYPE::FLOAT_16 && out_type == DATA_TYPE::FLOAT_16) {
		kernel_idx = DNN_fc_matrix_mul_partial_checked_1024_f16_f16;
		kernel_end_idx = DNN_fc_matrix_mul_partial_checked_32_f16_f16 + 2;
	} else {
		log_error("invalid in/out data type combination: %u, %u", in_type, out_type);
		return;
	}
	
	while ((dnn_max_local_sizes[kernel_idx] < local_size ||
			(fc_height % local_size) != 0) &&
		   kernel_idx < kernel_end_idx) {
		kernel_idx += 2;
		local_size >>= 1;
	}
	if (local_size < 32 || kernel_idx >= __MAX_DNN_KERNEL) {
		log_error("did not find a matching fully connected (partials) kernel");
		return;
	}
	if ((fc_width % local_size) == 0) {
		// FC width is a multiple of local size -> can use unchecked
		++kernel_idx;
	}
	
	const auto partials_count = (fc_height / local_size) + ((fc_height % local_size) != 0 ? 1 : 0);
	auto partials_buf = dnn_state.ctx->create_buffer(*dev_queue, out_type_size * partials_count * fc_width, COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	log_msg("partials kernel: %u, %u (w: %u, h: %u), partials: %u",
			kernel_idx, local_size, fc_width, fc_height, partials_count);
	log_msg("buffer sizes: %u, %u, %u, %u",
			(*input)->get_size(), fc.matrix->get_size(), partials_buf->get_size(), stage_output->get_size());
	dev_queue->execute(dnn_kernels[kernel_idx],
					   uint1 { fc_height },
					   uint1 { local_size },
					   *input, fc.matrix, partials_buf, fc_height, fc_width, partials_count);
	
	dev_queue->finish();
	
	dev_queue->execute(dnn_kernels[out_type == DATA_TYPE::FLOAT_32 ? DNN_fc_matrix_mul_total_f32 : DNN_fc_matrix_mul_total_f16],
					   uint1 { fc_width },
					   uint1 { local_size },
					   partials_buf, stage_output, fc.biases, partials_count, fc_width, with_relu ? 1u : 0u);
	
	dev_queue->finish();
	
	// update for next stage
	stage_input = stage_output;
	stage_output = nullptr;
	cur_input = stage_input.get();
	cur_dim = { fc_width, 1, 1 };
	cur_data_type = out_type;
	
	if (dump_output) {
		dump(layer_name);
	}
}

void nn_executer::softmax(const bool dump_output) {
	auto input = get_if<const compute_buffer*>(&cur_input);
	if (input == nullptr) {
		log_error("no input buffer set or unsupported image input");
		return;
	}
	
	// TODO: non-inplace softmax
	
	// NOTE: input will stay the same if in_type == out_type
	const auto in_type = cur_data_type;
	const auto out_type = DATA_TYPE::FLOAT_32; // always 32-bit float for now
	const auto out_type_size = data_type_size(out_type);
	
	const compute_buffer* out_buffer = *input;
	stage_output = nullptr;
	if (in_type != out_type) {
		stage_output = dnn_state.ctx->create_buffer(*dev_queue, out_type_size * cur_dim.x,
													COMPUTE_MEMORY_FLAG::READ_WRITE | /*(dump_output ?*/
																					   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE /*:
																					   COMPUTE_MEMORY_FLAG::HOST_WRITE)*/);
		out_buffer = stage_output.get();
	}
	
	uint32_t local_size = 1024;
	uint32_t kernel_idx = 0, kernel_end_idx = 0;
	if (in_type == DATA_TYPE::FLOAT_32 && out_type == DATA_TYPE::FLOAT_32) {
		kernel_idx = DNN_softmax_inplace_1024_f32_f32;
		kernel_end_idx = DNN_softmax_inplace_256_f32_f32 + 1;
	} else if (in_type == DATA_TYPE::FLOAT_16 && out_type == DATA_TYPE::FLOAT_32) {
		kernel_idx = DNN_softmax_inplace_1024_f16_f32;
		kernel_end_idx = DNN_softmax_inplace_256_f16_f32 + 1;
	} else {
		log_error("invalid in/out data type combination: %u, %u", in_type, out_type);
		return;
	}
	
	while ((dnn_max_local_sizes[kernel_idx] < local_size) &&
		   kernel_idx < kernel_end_idx) {
		kernel_idx += 1;
		local_size >>= 1;
	}
	if (local_size < 256 || kernel_idx >= __MAX_DNN_KERNEL) {
		log_error("did not find a matching fully connected (partials) kernel");
		return;
	}
	log_msg("softmax: local size: %u, idx %u", local_size, kernel_idx);
	
	dev_queue->execute(dnn_kernels[kernel_idx],
					   uint1 { local_size },
					   uint1 { local_size },
					   *input, out_buffer, cur_dim.x);
	dev_queue->finish();
	
	if (dump_output) {
		dump("softmax", cur_dim.x);
	}
	
	// update for next stage
	if (stage_output != nullptr) {
		stage_input = stage_output;
		stage_output = nullptr;
		cur_input = stage_input.get();
		cur_dim = { cur_dim.x, 1, 1 };
		cur_data_type = out_type;
	}
}
