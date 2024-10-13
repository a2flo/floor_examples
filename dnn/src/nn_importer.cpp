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

#include "nn_importer.hpp"

namespace nn_importer {
	template <typename layer_type>
	bool load_with_layer_type(const string& file_name, const string& layer_name, nn_model& mdl, const bool dump = false) {
		auto data = file_io::file_to_vector<uint8_t>(floor::data_path(file_name));
		log_debug("importing $ ...", file_name);
		if (!data) {
			return false;
		}
		
		// check size if it's known statically
		if (layer_type::is_serialization_size_static()) {
			if (layer_type::static_serialization_size() > data->size()) {
				log_error("insufficent data for $ layer $: expected $ bytes, got $",
						  file_name, layer_name, layer_type::static_serialization_size(), data->size());
				return false;
			}
		}
		
		// prefer read-only storage because it's a lot faster
		serializer<read_only_serializer_storage> ser {
			read_only_serializer_storage {
				*data,
				&(*data)[0],
				(&(*data)[data->size() - 1]) + 1,
			}
		};
		
		const auto add_layer = [&layer_name, &mdl, &dump](const auto& layer) {
			if (dump) {
				layer.dump();
			}
			return mdl.add_layer(layer_name, layer.create_compute_layer());
		};
		
		auto layer = layer_type::deserialize_dynamic(ser);
		if (!layer || !add_layer(*layer)) return false;
		
		return true;
	}
	
	optional<nn_model> import_vgg16(const bool load_fp16) {
		nn_model mdl;
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 3, 64>>("nets/vgg16/conv1_1_combined.bin", "conv1_1", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 64, 64>>("nets/vgg16/conv1_2_combined.bin", "conv1_2", mdl)) {
			return {};
		}
		if (!load_with_layer_type<conv_layer_t<3, 3, 64, 128>>("nets/vgg16/conv2_1_combined.bin", "conv2_1", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 128, 128>>("nets/vgg16/conv2_2_combined.bin", "conv2_2", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 128, 256>>("nets/vgg16/conv3_1_combined.bin", "conv3_1", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 256, 256>>("nets/vgg16/conv3_2_combined.bin", "conv3_2", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 256, 256>>("nets/vgg16/conv3_3_combined.bin", "conv3_3", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 256, 512>>("nets/vgg16/conv4_1_combined.bin", "conv4_1", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 512, 512>>("nets/vgg16/conv4_2_combined.bin", "conv4_2", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 512, 512>>("nets/vgg16/conv4_3_combined.bin", "conv4_3", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 512, 512>>("nets/vgg16/conv5_1_combined.bin", "conv5_1", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 512, 512>>("nets/vgg16/conv5_2_combined.bin", "conv5_2", mdl)) {
			return {};
		}
		
		if (!load_with_layer_type<conv_layer_t<3, 3, 512, 512>>("nets/vgg16/conv5_3_combined.bin", "conv5_3", mdl)) {
			return {};
		}
		
		if (!load_fp16) {
			if (!load_with_layer_type<fully_connected_layer_t<4096, 25088>>("nets/vgg16/fc6_combined.bin", "fc6", mdl)) {
				return {};
			}
			
			if (!load_with_layer_type<fully_connected_layer_t<4096, 4096>>("nets/vgg16/fc7_combined.bin", "fc7", mdl)) {
				return {};
			}
			
			if (!load_with_layer_type<fully_connected_layer_t<1000, 4096>>("nets/vgg16/fc8_combined.bin", "fc8", mdl)) {
				return {};
			}
		} else {
			if (!load_with_layer_type<fully_connected_layer_t<4096, 25088, half>>("nets/vgg16/fc6_combined_f16.bin", "fc6", mdl)) {
				return {};
			}
			
			if (!load_with_layer_type<fully_connected_layer_t<4096, 4096, half>>("nets/vgg16/fc7_combined_f16.bin", "fc7", mdl)) {
				return {};
			}
			
			if (!load_with_layer_type<fully_connected_layer_t<1000, 4096, half>>("nets/vgg16/fc8_combined_f16.bin", "fc8", mdl)) {
				return {};
			}
		}
		
		return mdl;
	}
	
}
