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

#include <floor/floor/floor.hpp>
#include <floor/core/option_handler.hpp>
#include "nn_importer.hpp"
#include "nn_executer.hpp"
#include "dnn_state.hpp"
#include "../../common/obj/obj_loader.hpp"
#if defined(FLOOR_IOS)
#include "ios/CamViewController.h"
#endif

#if defined(__APPLE__)
#include <SDL2_image/SDL_image.h>
#elif defined(__WINDOWS__)
#include <SDL2/SDL_image.h>
#else
#include <SDL_image.h>
#endif

dnn_state_struct dnn_state;
static constexpr const uint2 expected_img_dim { 224, 224 };

struct dnn_option_context {
	string image_file_name { "nets/test/tiger_rgba.png" };
	bool load_fp16 { false };
	// unused
	string additional_options { "" };
};
typedef option_handler<dnn_option_context> dnn_opt_handler;

//! option -> function map
template<> vector<pair<string, dnn_opt_handler::option_function>> dnn_opt_handler::options {
	{ "--help", [](dnn_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--image <file-name>: uses the specified RGBA image file instead of the default one" << endl;
		cout << "\t--benchmark: runs the dnn in benchmark mode (TODO/WIP)" << endl;
		cout << "\t--fp16: loads the fp16 version of the net" << endl;
		dnn_state.done = true;
	}},
	{ "--benchmark", [](dnn_option_context&, char**&) {
		dnn_state.benchmark = true;
		cout << "benchmark mode enabled" << endl;
	}},
	{ "--image", [](dnn_option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --image!" << endl;
			dnn_state.done = true;
			return;
		}
		ctx.image_file_name = *arg_ptr;
	}},
	{ "--fp16", [](dnn_option_context& ctx, char**&) {
		ctx.load_fp16 = true;
		cout << "loading fp16 net" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](dnn_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		dnn_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_ESCAPE:
			case SDLK_q:
				dnn_state.done = true;
				break;
			case SDLK_c:
				nn_executer::rebuild_kernels();
				break;
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::KEY_DOWN) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			default: break;
		}
		return true;
	}
	return false;
}

#if !defined(FLOOR_IOS)
static shared_ptr<compute_image> load_test_image(const string& file_name) {
	if (file_name.empty()) return {};
	
	const auto tex = obj_loader::load_texture(file_name[0] == '/' ? file_name : floor::data_path(file_name));
	if (!tex.first) {
		log_error("failed to load image %s", file_name);
		return {};
	}
	SDL_Surface* surface = tex.second.surface;
	
	//
	COMPUTE_IMAGE_TYPE image_type = obj_loader::floor_image_type_format(surface);
	image_type |= (COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::READ);
	
	// need to copy the data to a continuous array
	const uint2 dim { uint32_t(surface->w), uint32_t(surface->h) };
	const auto img_size = image_data_size_from_types(dim, image_type);
	auto pixels = make_unique<uint8_t[]>(img_size);
	memcpy(pixels.get(), surface->pixels, img_size);
	
	return dnn_state.ctx->create_image(dnn_state.dev,
									   dim,
									   image_type,
									   pixels.get(),
									   COMPUTE_MEMORY_FLAG::READ |
									   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
}
#endif

static optional<string> run_vgg(nn_executer& nn_exec, shared_ptr<compute_image> img) {
	// read imagenet classes
	static string classes_data;
	static once_flag did_load_classes;
	call_once(did_load_classes, []() {
		if (!file_io::file_to_string(floor::data_path("nets/vgg16/imagenet_classes.txt"), classes_data)) {
			log_error("failed to read imagenet classes - aborting ...");
			return;
		}
	});
	if (classes_data.empty()) {
		return {};
	}

	const auto classes_tokens = core::tokenize(classes_data, '\n');
	log_debug("#image-net classes: %u", classes_tokens.size());
	
	//
	shared_ptr<compute_image> input_img = img;
	if (!(img->get_image_dim().trim<2>() == expected_img_dim)) {
		log_debug("resampling image ...");
		input_img = nn_exec.resample(img);
	}
	
	// profiling
	dnn_state.dev_queue->start_profiling();

	//
	nn_exec.clear();
	nn_exec.input(*input_img);

	nn_exec.convolution("conv1_1");
	nn_exec.convolution("conv1_2");
	nn_exec.max_pooling();

	nn_exec.convolution("conv2_1");
	nn_exec.convolution("conv2_2");
	nn_exec.max_pooling();

	nn_exec.convolution("conv3_1");
	nn_exec.convolution("conv3_2");
	nn_exec.convolution("conv3_3");
	nn_exec.max_pooling();

	nn_exec.convolution("conv4_1");
	nn_exec.convolution("conv4_2");
	nn_exec.convolution("conv4_3");
	nn_exec.max_pooling();

	nn_exec.convolution("conv5_1");
	nn_exec.convolution("conv5_2");
	nn_exec.convolution("conv5_3");
	nn_exec.max_pooling();

	// TODO: fully_connected_relu / convolution_relu?
	nn_exec.fully_connected("fc6", true);
	nn_exec.fully_connected("fc7", true);
	nn_exec.fully_connected("fc8", false);

	nn_exec.softmax();
	
	// profiling end
	const auto vgg_exec_time = dnn_state.dev_queue->stop_profiling();
	log_msg("VGG16 executed in %ums", double(vgg_exec_time) / 1000.0);

	// eval/read results
	auto readback_buffer = nn_exec.get_cur_buffer()->clone(dnn_state.dev_queue, true,
														   COMPUTE_MEMORY_FLAG::READ_WRITE |
														   COMPUTE_MEMORY_FLAG::HOST_READ);
	
	const uint32_t prob_count = nn_exec.get_cur_dim().x;
	vector<float> probabilities(prob_count);
	vector<pair<uint32_t, float>> kv_probabilities;
	readback_buffer->read(dnn_state.dev_queue, &probabilities[0]);
	for (uint32_t i = 0; i < prob_count; ++i) {
		kv_probabilities.emplace_back(i, probabilities[i]);
	}

	sort(kv_probabilities.begin(), kv_probabilities.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.second > rhs.second;
	});

	// print top 5
	for (uint32_t i = 0; i < 5; ++i) {
		log_msg("#%u: %f -> %u (%s)", i, kv_probabilities[i].second * 100.0f, kv_probabilities[i].first,
				kv_probabilities[i].first < classes_tokens.size() ?
				classes_tokens[kv_probabilities[i].first] : "<out-of-bounds>");
	}
	
	// return top 3
	stringstream entries;
	entries.precision(3);
	for (uint32_t i = 0; i < 3; ++i) {
		entries << (kv_probabilities[i].first < classes_tokens.size() ?
					classes_tokens[kv_probabilities[i].first] : "<out-of-bounds>");
		entries << " (" << (kv_probabilities[i].second * 100.0f) << "%)" << endl;
	}
	return { entries.str() };
}

int main(int, char* argv[]) {
	// handle options
	dnn_option_context option_ctx;
	dnn_opt_handler::parse_options(argv + 1, option_ctx);
	if(dnn_state.done) return 0;
	
	// init floor
	if(!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "dnn",
#if !defined(FLOOR_IOS)
		.console_only = true,
#else
		.console_only = false,
#endif
		.renderer = floor::RENDERER::NONE, // opengl/vulkan/metal renderer are disabled
	})) {
		return -1;
	}
	
	// floor context handling
	struct floor_ctx_guard {
		floor_ctx_guard() {
			floor::acquire_context();
		}
		~floor_ctx_guard() {
			floor::release_context();
		}
	};
	event::handler evt_handler_fnctr(&evt_handler);
	{
		floor_ctx_guard grd;
	
		// add event handlers
		floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
													   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN);
		
		// get the compute context that has been automatically created (opencl/cuda/metal/vulkan/host)
		dnn_state.ctx = floor::get_compute_context();
		
		// create a compute queue (aka command queue or stream) for the fastest device in the context
		dnn_state.dev = dnn_state.ctx->get_device(compute_device::TYPE::FASTEST);
		dnn_state.dev_queue = dnn_state.ctx->create_queue(dnn_state.dev);
		
		// compile the program and get the kernel functions
		if(!nn_executer::init_kernels()) return -1;
		
		// init done, release context
	}
	
	do { // scoped for auto-cleanup
		// init/load vgg16
		auto vgg_mdl = nn_importer::import_vgg16(
#if !defined(FLOOR_IOS)
												 option_ctx.load_fp16
#else
												 true /* always load fp16 on iOS (due to memory limitations) */
#endif
		);
		if (!vgg_mdl) break;
		
		auto nn_exec = make_unique<nn_executer>(*vgg_mdl, dnn_state.dev, dnn_state.dev_queue);
		
#if !defined(FLOOR_IOS)
		// load test image
		auto input_img = load_test_image(option_ctx.image_file_name);
		if (!input_img) break;
		
		if ((input_img->get_image_dim().trim<2>() != expected_img_dim).any()) {
			log_error("unexpected image dim: expected %v, got %v",
					  expected_img_dim, input_img->get_image_dim().trim<2>());
			break;
		}
		
		uint32_t iteration = 0;
#else
		// use a camera image on iOS
		init_cam_view(floor::get_window(), [&nn_exec](shared_ptr<compute_image> img) -> string {
			if (img == nullptr) {
				return "<null-img>";
			}
			
			auto ret = run_vgg(*nn_exec, img);
			if (!ret) {
				return "<failure>";
			}
			return *ret;
		});
#endif
		
		// main loop
		while (!dnn_state.done) {
			floor::get_event()->handle_events();
			
#if !defined(FLOOR_IOS)
			log_debug("running VGG ... ");
			run_vgg(*nn_exec, input_img);
			
			// next
			++iteration;
			
			// when benchmarking, we need to make sure computation is actually finished (all 20 iterations were computed)
			if(dnn_state.benchmark && iteration == 20) {
				dnn_state.dev_queue->finish();
				break;
			}
			
			// TODO/NOTE: for now, only run this once and exit (in benchmark mode, should run multiple times)
			dnn_state.done = true;
#endif
		}
	} while (false);
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	// cleanup
	floor::acquire_context();
	floor::release_context();
	
	// kthxbye
	floor::destroy();
	return 0;
}
