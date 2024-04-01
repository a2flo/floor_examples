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

#ifndef __FLOOR_DNN_NN_IMPORTER_HPP__
#define __FLOOR_DNN_NN_IMPORTER_HPP__

#include <floor/floor/floor.hpp>
#include <floor/core/serializer.hpp>
#include <floor/core/serializer_storage.hpp>
#include "dnn_state.hpp"

enum class DATA_FORMAT : uint32_t {
	NHWC = 0,
	NCHW = 1,
	//! filters as type[width][height][#in-channels][#out-channels] C array
	OIHW = 2,
	//! filters as type[#out-channels][#in-channels][height][width] C array
	WHIO = 3,
};

enum class DATA_TYPE : uint32_t {
	FLOAT_32 = 0,
	FLOAT_16 = 1,
};

template <typename type>
static constexpr inline DATA_TYPE type_to_data_type() {
	if constexpr (is_same_v<type, float>) {
		return DATA_TYPE::FLOAT_32;
	} else if constexpr (is_same_v<type, half>) {
		return DATA_TYPE::FLOAT_16;
	}
	// fail if not covered
}

static constexpr inline size_t data_type_size(const DATA_TYPE& data_type) {
	switch (data_type) {
		case DATA_TYPE::FLOAT_32: return 4;
		case DATA_TYPE::FLOAT_16: return 2;
	}
	return 0;
}


struct compute_conv_layer_t {
	DATA_TYPE type;
	DATA_FORMAT format;
	uint4 shape;
	shared_ptr<compute_buffer> filters;
	shared_ptr<compute_buffer> biases;
	
	uint32_t get_width() const {
		return shape.x;
	}
	uint32_t get_height() const {
		return shape.y;
	}
	uint32_t get_input_channels() const {
		return shape.z;
	}
	uint32_t get_output_channels() const {
		return shape.w;
	}
};

struct compute_fully_connected_layer_t {
	DATA_TYPE type;
	uint2 shape;
	shared_ptr<compute_buffer> matrix;
	shared_ptr<compute_buffer> biases;
	
	uint32_t get_width() const {
		return shape.x;
	}
	uint32_t get_height() const {
		return shape.y;
	}
};

enum class LAYER_TYPE : uint32_t {
	CONVOLUTION,
	FULLY_CONNECTED,
};

//
template <uint32_t width, uint32_t height, uint32_t input_channels, uint32_t output_channels,
		  DATA_FORMAT format = DATA_FORMAT::WHIO>
struct conv_layer_t {
	static constexpr LAYER_TYPE layer_type() { return LAYER_TYPE::CONVOLUTION; }
	
	static_assert(format == DATA_FORMAT::OIHW || format == DATA_FORMAT::WHIO, "format must be OIHW or WHIO");
	using data_format_t = conditional_t<format == DATA_FORMAT::WHIO,
		array<array<array<array<float, output_channels>, input_channels>, height>, width>,
		array<array<array<array<float, width>, height>, input_channels>, output_channels>
	>;
	
	data_format_t filters;
	
	array<float, output_channels> biases;
	
	void dump() const {
		stringstream stream;
		stream << fixed << scientific;
		stream.precision(5);
		
		stream << "weights = {" << endl;
		
		for (uint32_t och = 0; och < output_channels; ++och) {
			stream << "\t{" << endl;
			for (uint32_t ich = 0; ich < input_channels; ++ich) {
				stream << "\t\t{" << endl;
				const string indent_str(3, '\t');
				for (uint32_t y = 0; y < height; ++y) {
					stream << indent_str << "|";
					for (uint32_t x = 0; x < width; ++x) {
						if constexpr (format == DATA_FORMAT::OIHW) {
							stream << '\t' << filters[x][y][ich][och];
						} else {
							stream << '\t' << filters[och][ich][y][x];
						}
					}
					stream << "\t|" << endl;
				}
				
				stream << "\t\t}," << endl;
			}
			stream << "\t}," << endl;
		}
		
		stream << "}," << endl;
		stream << "biases = {" << endl;
		for (const auto& bias : biases) {
			stream << "\t" << bias << "," << endl;
		}
		stream << "}" << endl;
		log_undecorated("$", stream.str());
	}
	
	static constexpr uint4 shape() {
		return { width, height, input_channels, output_channels };
	}
	
	constexpr uint32_t get_width() const {
		return width;
	}
	constexpr uint32_t get_height() const {
		return height;
	}
	constexpr uint32_t get_input_channels() const {
		return input_channels;
	}
	constexpr uint32_t get_output_channels() const {
		return output_channels;
	}
	
	compute_conv_layer_t create_compute_layer() const {
		auto filters_buf = dnn_state.ctx->create_buffer(*dnn_state.dev_queue, span<const uint8_t> { (const uint8_t*)&filters, sizeof(decltype(filters)) },
														COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
		auto biases_buf = dnn_state.ctx->create_buffer(*dnn_state.dev_queue, span<const uint8_t> { (const uint8_t*)&biases, sizeof(decltype(biases)) },
													   COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
		
		return {
			.type = DATA_TYPE::FLOAT_32,
			.format = format,
			.shape = shape(),
			.filters = filters_buf,
			.biases = biases_buf,
		};
	}
	
	template <DATA_FORMAT conv_format>
	auto convert() const {
		if constexpr (conv_format == format) {
			return *this;
		} else {
			conv_layer_t<width, height, input_channels, output_channels, conv_format> conv;
			conv.biases = biases; // these stay the same
			
			if constexpr (format == DATA_FORMAT::WHIO && conv_format == DATA_FORMAT::OIHW) {
				for (uint32_t och = 0; och < output_channels; ++och) {
					for (uint32_t ich = 0; ich < input_channels; ++ich) {
						for (uint32_t y = 0; y < height; ++y) {
							for (uint32_t x = 0; x < width; ++x) {
								conv.filters[och][ich].weights[y * width + x] = filters[x][y][ich][och];
							}
						}
					}
				}
			} else if constexpr (format == DATA_FORMAT::OIHW && conv_format == DATA_FORMAT::WHIO) {
				for (uint32_t och = 0; och < output_channels; ++och) {
					for (uint32_t ich = 0; ich < input_channels; ++ich) {
						for (uint32_t y = 0; y < height; ++y) {
							for (uint32_t x = 0; x < width; ++x) {
								conv.filters[x][y][ich][och] = filters[och][ich].weights[y * width + x];
							}
						}
					}
				}
			}
			
			return conv;
		}
	}
	
	template <DATA_FORMAT conv_format>
	auto convert_dynamic() const {
		if constexpr (conv_format == format) {
			auto ret = make_unique<decltype(*this)>();
			ret *= *this;
			return ret;
		} else {
			auto conv = make_unique<conv_layer_t<width, height, input_channels, output_channels, conv_format>>();
			conv->biases = biases; // these stay the same
			
			if constexpr (format == DATA_FORMAT::WHIO && conv_format == DATA_FORMAT::OIHW) {
				for (uint32_t och = 0; och < output_channels; ++och) {
					for (uint32_t ich = 0; ich < input_channels; ++ich) {
						for (uint32_t y = 0; y < height; ++y) {
							for (uint32_t x = 0; x < width; ++x) {
								conv->filters[och][ich].weights[y * width + x] = filters[x][y][ich][och];
							}
						}
					}
				}
			} else if constexpr (format == DATA_FORMAT::OIHW && conv_format == DATA_FORMAT::WHIO) {
				for (uint32_t och = 0; och < output_channels; ++och) {
					for (uint32_t ich = 0; ich < input_channels; ++ich) {
						for (uint32_t y = 0; y < height; ++y) {
							for (uint32_t x = 0; x < width; ++x) {
								conv->filters[x][y][ich][och] = filters[och][ich].weights[y * width + x];
							}
						}
					}
				}
			}
			
			return conv;
		}
	}
	
	SERIALIZATION(conv_layer_t, filters, biases)
};

//
template <uint32_t width, uint32_t height, typename data_type = float>
struct fully_connected_layer_t {
	static constexpr LAYER_TYPE layer_type() { return LAYER_TYPE::FULLY_CONNECTED; }
	
	array<array<data_type, width>, height> matrix;
	array<data_type, width> biases;
	
	static constexpr uint2 shape() {
		return { width, height };
	}
	
	void dump() const {
		// TODO
	}
	
	compute_fully_connected_layer_t create_compute_layer() const {
		auto matrix_buf = dnn_state.ctx->create_buffer(*dnn_state.dev_queue, span<const uint8_t> { (const uint8_t*)&matrix, sizeof(decltype(matrix)) },
													   COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
		auto biases_buf = dnn_state.ctx->create_buffer(*dnn_state.dev_queue, span<const uint8_t> { (const uint8_t*)&biases, sizeof(decltype(biases)) },
													   COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
		
		return {
			.type = type_to_data_type<data_type>(),
			.shape = shape(),
			.matrix = matrix_buf,
			.biases = biases_buf,
		};
	}
	
	SERIALIZATION(fully_connected_layer_t, matrix, biases)
};

//
template <uint32_t count>
struct test_dummy {
	array<float, count> data;
	
	void dump() const {
		stringstream stream;
		stream << fixed << scientific;
		stream.precision(5);
		for (uint32_t i = 0; i < count; ++i) {
			stream << i << ": " << data[i] << endl;
		}
		log_undecorated("$", stream.str());
	}
	
	SERIALIZATION(test_dummy, data)
};

struct nn_model {
	unordered_map<string, compute_conv_layer_t> conv_layers;
	unordered_map<string, compute_fully_connected_layer_t> fully_connected_layers;
	
	optional<const compute_conv_layer_t*> get_conv_layer(const string& name) const {
		const auto iter = conv_layers.find(name);
		if (iter == conv_layers.end()) {
			return {};
		}
		return { &iter->second };
	}
	
	optional<const compute_fully_connected_layer_t*> get_fully_connected_layer(const string& name) const {
		const auto iter = fully_connected_layers.find(name);
		if (iter == fully_connected_layers.end()) {
			return {};
		}
		return { &iter->second };
	}
	
	template <typename layer_type>
	bool add_layer(const string& name, layer_type&& layer) {
		if constexpr (is_same_v<layer_type, compute_conv_layer_t>) {
			auto [iter, did_emplace] = conv_layers.emplace(name, std::forward<layer_type>(layer));
			return did_emplace;
		} else if constexpr (is_same_v<layer_type, compute_fully_connected_layer_t>) {
			auto [iter, did_emplace] = fully_connected_layers.emplace(name, std::forward<layer_type>(layer));
			return did_emplace;
		} else {
			return false;
		}
	}
	
};

namespace nn_importer {
	optional<nn_model> import_vgg16(const bool load_fp16 = false);

}

#endif
