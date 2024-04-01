/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_DNN_NN_EXECUTER_HPP__
#define __FLOOR_DNN_NN_EXECUTER_HPP__

#include "dnn_state.hpp"
#include "nn_importer.hpp"
#include <variant>

class nn_executer {
public:
	nn_executer(const nn_model& mdl, const compute_device& dev, shared_ptr<compute_queue> dev_queue);
	
	//! clear all previous execution state
	void clear();
	
	//! set new NN input state (either a buffer or an image)
	void input(const compute_image& img, const bool rgba_as_rgb = true);
	void input(const compute_buffer& buf, const uint3 dim);
	
	//! performs the specified/named 3x3 convolution with bias and relu
	void convolution(const string& layer_name, const bool dump_output = false);
	
	//! performs 2x2 max pooling
	void max_pooling(const bool dump_output = false);
	
	//! performs the specified/name fully-connected layer computation (matrix multiplication) with optional relu
	void fully_connected(const string& layer_name, const bool with_relu, const bool dump_output = false);
	
	//! performs a softmax (exp)
	void softmax(const bool dump_output = false);
	
	//! resamples the specified image to a 224*224 image
	shared_ptr<compute_image> resample(shared_ptr<compute_image> img);
	
	//! dumps the current data as 32-bit float up to the specified amount of elements (all if ~0ull)
	void dump(const string name = "", const uint64_t max_elems = ~0ull);
	
	//! return the current buffer dim
	const uint3& get_cur_dim() const {
		return cur_dim;
	}
	
	//! return the current buffer
	shared_ptr<compute_buffer> get_cur_buffer() {
		return stage_input;
	}
	
	//! global init of the program/kernels
	static bool init_kernels();
	//! rebuild/reload all kernels
	static bool rebuild_kernels();
	
protected:
	const nn_model& mdl;
	const compute_device& dev;
	shared_ptr<compute_queue> dev_queue;
	
	const compute_image* input_img { nullptr };
	const compute_buffer* input_buf { nullptr };
	uint3 input_dim; // (width, height, channels)
	
	shared_ptr<compute_buffer> stage_input;
	shared_ptr<compute_buffer> stage_output;
	
	variant<const compute_buffer*, const compute_image*> cur_input;
	uint3 cur_dim;
	DATA_TYPE cur_data_type { DATA_TYPE::FLOAT_32 };
	
};

#endif
