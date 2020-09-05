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

#ifndef __FLOOR_WARP_METAL_RENDERER_HPP__
#define __FLOOR_WARP_METAL_RENDERER_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include "common_renderer.hpp"

class metal_renderer final : public common_renderer {
public:
	bool init() override;
	void render(const floor_obj_model& model, const camera& cam) override;
	
protected:
	void create_textures(const COMPUTE_IMAGE_TYPE color_format) override;
	bool compile_shaders(const string add_cli_options) override;
	
	void render_full_scene(const floor_obj_model& model, const camera& cam) override;
	
};

#endif

#endif
