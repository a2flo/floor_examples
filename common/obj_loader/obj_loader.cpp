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

#include "obj_loader.hpp"

//#define FLOOR_DEBUG_PARSER 1
//#define FLOOR_DEBUG_PARSER_SET_NAMES 1
#include <floor/lang/source_types.hpp>
#include <floor/lang/lang_context.hpp>
#include <floor/lang/grammar.hpp>
#include <floor/constexpr/const_string.hpp>
#include <floor/core/core.hpp>
#include <floor/threading/task.hpp>

#include <SDL3_image/SDL_image.h>

#if defined(FLOOR_IOS)
#include <floor/darwin/darwin_helper.hpp>
#endif

// -> floor (for use with metal/vulkan)
#define __FLOOR_TEXTURE_FORMATS(F, src_surface, format_details, dst_image_type) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::R8UI_NORM, 8, 0, 0, 0, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RG8UI_NORM, 16, 0, 8, 0, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGB8UI_NORM, 24, 0, 8, 16, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGB8UI_NORM /* BGR converted to RGB */, 24, 16, 8, 0, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM, 32, 0, 8, 16, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM, 32, 0, 8, 16, 24) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM /* BGRA converted to RGBA */, 32, 16, 8, 0, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM /* BGRA converted to RGBA */, 32, 16, 8, 0, 24) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::R16UI_NORM, 16, 0, 0, 0, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RG16UI_NORM, 32, 0, 16, 0, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGB16UI_NORM, 48, 0, 16, 32, 0) \
F(src_surface, format_details, dst_image_type, COMPUTE_IMAGE_TYPE::RGBA16UI_NORM, 64, 0, 16, 32, 48)

#define __FLOOR_CHECK_FORMAT(src_surface, format_details, dst_image_type, floor_image_type, bpp, rshift, gshift, bshift, ashift) \
	if (format_details->Rshift == rshift && format_details->Gshift == gshift && format_details->Bshift == bshift && \
		format_details->Ashift == ashift && format_details->bits_per_pixel == bpp) { \
		dst_image_type = floor_image_type; \
	}

COMPUTE_IMAGE_TYPE obj_loader::floor_image_type_format(const SDL_Surface* surface) {
	COMPUTE_IMAGE_TYPE image_type { COMPUTE_IMAGE_TYPE::NONE };
	const auto px_format_details = SDL_GetPixelFormatDetails(surface->format);
	__FLOOR_TEXTURE_FORMATS(__FLOOR_CHECK_FORMAT, surface, px_format_details, image_type)
	if (image_type == COMPUTE_IMAGE_TYPE::NONE) {
		log_error("unknown surface format: $, $, $, $, $",
				  (uint32_t)px_format_details->bits_per_pixel,
				  (uint32_t)px_format_details->Rshift,
				  (uint32_t)px_format_details->Gshift,
				  (uint32_t)px_format_details->Bshift,
				  (uint32_t)px_format_details->Ashift);
	}
	return image_type;
}

class obj_lexer final : public lexer {
public:
	static void lex(translation_unit& tu);
	
	//! assigns the resp. FLOOR_KEYWORD and FLOOR_PUNCTUATOR enums/sub-type to the token type
	static void assign_token_sub_types(translation_unit& tu);
	
protected:
	static lex_return_type lex_keyword_or_identifier(const translation_unit& tu,
													 source_iterator& iter,
													 const source_iterator& source_end);
	static lex_return_type lex_decimal_constant(const translation_unit& tu,
												source_iterator& iter,
												const source_iterator& source_end);
	static lex_return_type lex_comment(const translation_unit& tu,
									   source_iterator& iter,
									   const source_iterator& source_end);
	
	// static class
	obj_lexer(const obj_lexer&) = delete;
	~obj_lexer() = delete;
	obj_lexer& operator=(const obj_lexer&) = delete;
	
};

//! contains all valid punctuators
static const unordered_map<string, FLOOR_PUNCTUATOR> punctuator_tokens {
	{ "/", FLOOR_PUNCTUATOR::DIV },
};

void obj_lexer::lex(translation_unit& tu) {
	// tokens reserve strategy: "4 chars : 1 token" seems like a good ratio for now
	tu.tokens.reserve(tu.source.size() / 4);
	
	// lex
	for(const char* src_begin = tu.source.data(), *src_end = tu.source.data() + tu.source.size(), *char_iter = src_begin;
		char_iter != src_end;
		/* NOTE: char_iter is incremented in the individual lex_* functions or whitespace case: */) {
		switch(*char_iter) {
			// '-' can start either decimal constant or keyword/identifier (in .mtl, always decimal in .obj)
			case '-': {
				auto next_char = char_iter + 1;
				if(next_char != src_end) {
					switch(*next_char) {
						case '0': case '1': case '2': case '3':
						case '4': case '5': case '6': case '7':
						case '8': case '9':
							goto decimal_constant;
						default: break;
					}
				}
				goto keyword_or_identifier;
			}

			// keyword or identifier
			case '_': case '.':
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z': {
keyword_or_identifier:
				source_range range { char_iter, char_iter };
				const auto ret = lex_keyword_or_identifier(tu, char_iter, src_end);
				if(!ret.first) break;
				range.end = ret.second;
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::IDENTIFIER, range);
				break;
			}
				
			// decimal constant
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9': {
decimal_constant:
				source_range range { char_iter, char_iter };
				const auto ret = lex_decimal_constant(tu, char_iter, src_end);
				if(!ret.first) break;
				range.end = ret.second;
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::INTEGER_CONSTANT, range);
				break;
			}
				
			// '#' -> comment
			case '#': {
				// comment
				lex_comment(tu, char_iter, src_end);
				break;
			}
				
			// '/' -> separator/punctuator
			case '/': {
				source_range range { char_iter, char_iter + 1 };
				++char_iter;
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::PUNCTUATOR, range);
				break;
			}
				
			// whitespace
			// "space, horizontal tab, new-line, vertical tab, and form-feed"
			case ' ': case '\t': case '\n': case '\v':
			case '\f':
				// continue
				++char_iter;
				break;
				
			// invalid char
			default: {
				const string invalid_char = (is_printable_char(char_iter) ? string(1, *char_iter) : "<unprintable>");
				handle_error(tu, char_iter, "invalid character \'" + invalid_char + "\' (" +
							 to_string(0xFFu & (uint32_t)*char_iter) + ")");
				break;
			}
		}
	}
}

lexer::lex_return_type obj_lexer::lex_keyword_or_identifier(const translation_unit&,
															source_iterator& iter,
															const source_iterator& source_end) {
	for(++iter; iter != source_end; ++iter) {
		switch(*iter) {
			// valid keyword and identifier characters
			case '_': case '.': case '/': case '\\':
			case '-':
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9':
				continue;
				
			// anything else -> done, return end iter
			default:
				return { true, iter };
		}
	}
	return { true, iter }; // eof
}

lexer::lex_return_type obj_lexer::lex_decimal_constant(const translation_unit&,
													   source_iterator& iter,
													   const source_iterator& source_end) {
	for(++iter; iter != source_end; ++iter) {
		switch(*iter) {
			// valid decimal constant characters
			case '.': case '-': case '+':
			case 'e': case 'E':
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9':
				continue;
				
			// anything else -> done, return end iter
			default:
				return { true, iter };
		}
	}
	return { true, iter }; // eof
}

lexer::lex_return_type obj_lexer::lex_comment(const translation_unit&,
											  source_iterator& iter,
											  const source_iterator& source_end) {
	// NOTE: we already made sure that this must be a comment
	++iter;
	
	// single-line comment
	for(++iter; iter != source_end; ++iter) {
		// newline signals end of single-line comment
		if(*iter == '\n') {
			return { true, iter };
		}
	}
	return { true, iter }; // eof is okay in a single-line comment
}

void obj_lexer::assign_token_sub_types(translation_unit& tu) {
	for(auto& token : tu.tokens) {
		// skip non-punctuators
		if(token.first != SOURCE_TOKEN_TYPE::PUNCTUATOR) {
			continue;
		}
		
		// handle punctuators
		token.first |= (SOURCE_TOKEN_TYPE)punctuator_tokens.find(token.second.to_string())->second;
	}
}

struct keyword_matcher : public parser_node_base<keyword_matcher> {
	const char* keyword;
	const size_t keyword_len;
	
	template <size_t len> constexpr keyword_matcher(const char (&keyword_)[len]) noexcept :
	keyword(keyword_), keyword_len(len - 1 /* -1, b/c of \0 */) {}
	
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching KEYWORD");
#endif
		if(!ctx.at_end() &&
		   ctx.iter->first == SOURCE_TOKEN_TYPE::IDENTIFIER &&
		   ctx.iter->second.equal(keyword, keyword_len)) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

struct mtl_grammar {
	// all grammar rule objects
#if !defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_SET_NAMES)
#define FLOOR_MTL_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__;
#else
#define FLOOR_MTL_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__; \
	/* in debug mode, also set the debug name of each grammar rule object */ \
	void set_debug_names() { \
		string names { #__VA_ARGS__ }; \
		set_debug_name(names, __VA_ARGS__); \
	} \
	void set_debug_name(string& names, grammar_rule& obj) { \
		const auto comma_pos = names.find(","); \
		obj.debug_name = names.substr(0, comma_pos); \
		names.erase(0, comma_pos + 1 + (comma_pos+1 < names.size() && names[comma_pos+1] == ' ' ? 1 : 0)); \
	} \
	template <typename... grammar_objects> void set_debug_name(string& names, grammar_rule& obj, grammar_objects&... objects) { \
		set_debug_name(names, obj); \
		set_debug_name(names, objects...); \
	}
#endif
	
	FLOOR_MTL_GRAMMAR_OBJECTS(global_scope, statement, newmtl,
							  ns, ni, d, tr, tf, illum,
							  ka, kd, ks, ke,
							  map_ka, map_kd, map_ks, map_ke, map_bump, map_d, disp,
							  map_options)
	
	struct obj_material {
		string ambient { "" };
		string diffuse { "" };
		string specular { "" };
		string normal { "" };
		string mask { "" };
		string displacement { "" };
	};
	unordered_map<string, unique_ptr<obj_material>> materials;
	obj_material* cur_mtl { nullptr };
	
	mtl_grammar() {
		// fixed token type matchers:
		static constexpr literal_matcher<const char*, SOURCE_TOKEN_TYPE::INTEGER_CONSTANT> FP_CONSTANT {};
		static constexpr literal_matcher<const char*, SOURCE_TOKEN_TYPE::IDENTIFIER> IDENTIFIER {};
		
		//
		static constexpr keyword_matcher NEWMTL("newmtl"), NS("Ns"), NI("Ni"), D("d"), TR("Tr"), TF("Tf"), ILLUM("illum"), KA("Ka"), KD("Kd"), KS("Ks"), KE("Ke"), MAP_KA("map_Ka"), MAP_KD("map_Kd"), MAP_KS("map_Ks"), MAP_KE("map_Ke"), MAP_BUMP("map_bump"), MAP_BUMP2("map_Bump"), BUMP("bump"), MAP_D("map_d"), DISP("disp");
		static constexpr keyword_matcher MAP_OPT_BM("-bm");
		
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
		set_debug_names();
#endif
		
		// grammar:
		// ref: http://paulbourke.net/dataformats/mtl/
		global_scope = *statement;
		statement = (newmtl | ns | ni | d | tr | tf | illum | ka | kd | ks | ke | map_ka | map_kd | map_ks | map_ke | map_bump | map_d | disp);
		newmtl = NEWMTL & IDENTIFIER;
		ns = NS & FP_CONSTANT;
		ni = NI & FP_CONSTANT;
		d = D & FP_CONSTANT;
		tr = TR & FP_CONSTANT;
		tf = TF & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT;
		illum = ILLUM & FP_CONSTANT;
		ka = KA & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT;
		kd = KD & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT;
		ks = KS & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT;
		ke = KE & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT;
		map_ka = MAP_KA & *map_options & IDENTIFIER;
		map_kd = MAP_KD & *map_options & IDENTIFIER;
		map_ks = MAP_KS & *map_options & IDENTIFIER;
		map_ke = MAP_KE & *map_options & IDENTIFIER;
		map_bump = (MAP_BUMP | MAP_BUMP2 | BUMP) & *map_options & IDENTIFIER;
		map_d = MAP_D & *map_options & IDENTIFIER;
		disp = DISP & *map_options & IDENTIFIER;
		map_options = MAP_OPT_BM & FP_CONSTANT; // TODO: more
		
		//
		newmtl.on_match([this](auto& matches) -> parser_context::match_list {
			const auto name = matches[1].token->second.to_string();
			auto mtl = make_unique<obj_material>();
			materials.emplace(name, std::move(mtl));
			cur_mtl = materials[name].get();
			return {};
		});
		map_ka.on_match([this](auto& matches) -> parser_context::match_list {
			cur_mtl->ambient = matches.back().token->second.to_string();
			return {};
		});
		map_kd.on_match([this](auto& matches) -> parser_context::match_list {
			cur_mtl->diffuse = matches.back().token->second.to_string();
			return {};
		});
		map_ks.on_match([this](auto& matches) -> parser_context::match_list {
			cur_mtl->specular = matches.back().token->second.to_string();
			return {};
		});
		map_bump.on_match([this](auto& matches) -> parser_context::match_list {
			cur_mtl->normal = matches.back().token->second.to_string();
			return {};
		});
		map_d.on_match([this](auto& matches) -> parser_context::match_list {
			cur_mtl->mask = matches.back().token->second.to_string();
			return {};
		});
		disp.on_match([this](auto& matches) -> parser_context::match_list {
			cur_mtl->displacement = matches.back().token->second.to_string();
			return {};
		});
		map_options.on_match([](auto& matches floor_unused) -> parser_context::match_list {
			return {};
		});
	}
	
	unordered_map<string, unique_ptr<obj_material>> parse(parser_context& ctx, bool& success) {
		// clear all
		materials.clear();
		success = false;
		
		// parse
		global_scope.match(ctx);
		
		// if the end hasn't been reached, we have an error
		if(ctx.iter != ctx.end) {
			string error_msg = "parsing failed: ";
			if(ctx.deepest_iter == ctx.tu.tokens.cend()) {
				error_msg += "premature EOF after";
				ctx.deepest_iter = ctx.end - 1; // set iter to token before EOF
			}
			else {
				error_msg += "possibly at";
			}
			error_msg += " \"" + ctx.deepest_iter->second.to_string() + "\"";
			
			const auto line_and_column = obj_lexer::get_line_and_column_from_iter(ctx.tu, ctx.deepest_iter->second.begin);
			log_error("$:$:$: $",
					  ctx.tu.file_name, line_and_column.first, line_and_column.second, error_msg);
			return {};
		}
#if defined(FLOOR_DEBUG)
		log_debug("mtl parsing done");
#endif
		
		// done
		success = true;
		return std::move(materials);
	}
};


pair<bool, obj_loader::texture> obj_loader::load_texture(const string& filename) {
	texture ret;
	
	if(filename.find(".pvr") == string::npos) {
		SDL_Surface* surface = IMG_Load(filename.c_str());
		if(surface == nullptr) {
			log_error("error loading texture file \"$\": $!", filename, SDL_GetError());
			return { false, {} };
		}
		
		// freeing sdl surfaces is not thread-safe
		static safe_mutex surface_free_mutex;
		const auto free_surface = [](SDL_Surface* surf) {
			GUARD(surface_free_mutex);
			SDL_DestroySurface(surf);
		};
		
		// check format, we always want RGB(A)
		// NOTE: SDL uses reverse order ... RGBA is ABGR in SDL
		optional<SDL_PixelFormat> conv_format;
		switch (surface->format) {
			default:
				break;
			case SDL_PIXELFORMAT_RGBA4444:
			case SDL_PIXELFORMAT_ARGB4444:
			case SDL_PIXELFORMAT_BGRA4444:
				conv_format = SDL_PIXELFORMAT_ABGR4444;
				break;
			case SDL_PIXELFORMAT_XRGB1555:
			case SDL_PIXELFORMAT_ARGB1555:
			case SDL_PIXELFORMAT_RGBA5551:
			case SDL_PIXELFORMAT_BGRA5551:
				conv_format = SDL_PIXELFORMAT_ABGR1555;
				break;
			case SDL_PIXELFORMAT_RGB565:
				conv_format = SDL_PIXELFORMAT_BGR565;
				break;
			case SDL_PIXELFORMAT_RGB24:
				conv_format = SDL_PIXELFORMAT_BGR24;
				break;
			case SDL_PIXELFORMAT_XRGB8888:
			case SDL_PIXELFORMAT_RGBX8888:
			case SDL_PIXELFORMAT_XBGR8888:
			case SDL_PIXELFORMAT_BGRX8888:
				conv_format = SDL_PIXELFORMAT_XBGR8888;
				break;
			case SDL_PIXELFORMAT_ARGB8888:
			case SDL_PIXELFORMAT_RGBA8888:
			case SDL_PIXELFORMAT_BGRA8888:
				conv_format = SDL_PIXELFORMAT_ABGR8888;
				break;
			case SDL_PIXELFORMAT_RGB48:
				conv_format = SDL_PIXELFORMAT_BGR48;
				break;
			case SDL_PIXELFORMAT_ARGB64:
			case SDL_PIXELFORMAT_RGBA64:
			case SDL_PIXELFORMAT_BGRA64:
				conv_format = SDL_PIXELFORMAT_ABGR64;
				break;
		}
		
		if (conv_format) {
			SDL_Surface* new_surface = SDL_ConvertSurface(surface, *conv_format);
			if (new_surface == nullptr) {
				log_error("BGR(A)->RGB(A) surface conversion failed!");
				free_surface(surface);
				return { false, {} };
			}
			free_surface(surface);
			surface = new_surface;
		}
		
		ret.surface = surface;
	}
	else {
		struct pvrtc_legacy_header {
			uint32_t header_size;
			uint32_t height;
			uint32_t width;
			uint32_t mip_map_count;
			struct {
				uint32_t format : 8;
				uint32_t flags : 24;
			};
			uint32_t surface_size;
			uint32_t bpp;
			uint32_t red_mask;
			uint32_t green_mask;
			uint32_t blue_mask;
			uint32_t alpha_mask;
			uint32_t pvr_id;
			uint32_t surface_count;
		};
		static constexpr const uint32_t pvrtc_header_size { 52 };
		static_assert(sizeof(pvrtc_legacy_header) == pvrtc_header_size, "invalid header size");
		
		string img_data;
		if(!file_io::file_to_string(filename, img_data)) {
			log_error("error loading texture file \"$\"!", filename);
			return { false, {} };
		}
		
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align)
		const auto header = (const pvrtc_legacy_header*)img_data.data();
FLOOR_POP_WARNINGS()
		
		// sanity checks
		if(header->header_size != pvrtc_header_size) {
			log_error("invalid pvrtc header size: got $, expected $!", header->header_size, pvrtc_header_size);
			return { false, {} };
		}
		if(header->pvr_id != 0x21525650u) {
			log_error("invalid pvrtc id: $X!", header->pvr_id);
			return { false, {} };
		}
		if(header->format != 0xC &&
		   header->format != 0xD &&
		   header->format != 0x18 &&
		   header->format != 0x19) {
			log_error("pvr pixel format ($X) is not PVRTC!", header->format);
			return { false, {} };
		}
		
		// fill ret data
		ret.pvrtc = make_shared<pvrtc_texture>(pvrtc_texture {
			.dim = { header->width, header->height },
			.bpp = header->bpp,
			.is_mipmapped = header->mip_map_count > 1,
			.is_alpha = (header->flags & 0x8000u) != 0u,
			.pixels = make_unique<uint8_t[]>(header->surface_size),
			.image_size = header->surface_size,
		});
		memcpy(ret.pvrtc->pixels.get(), img_data.data() + pvrtc_header_size, header->surface_size);
	}
	
	return { true, ret };
}

static void load_textures(// file name -> <gl tex id, compute image ptr>
						  unordered_map<string, pair<uint32_t, compute_image*>>& texture_filenames,
						  // metal/vulkan tex objects
						  vector<shared_ptr<compute_image>>* model_floor_textures,
						  // path prefix
						  const string& prefix,
						  const compute_context& ctx,
						  const compute_queue& cqueue) {
	// load textures
	const auto filenames = core::keys(texture_filenames);
	alignas(128) vector<obj_loader::texture> textures(filenames.size());
	atomic<uint32_t> cur_tex { 0u }, active_workers { core::get_hw_thread_count() };
	for(uint32_t worker_id = 0; worker_id < core::get_hw_thread_count(); ++worker_id) {
		task::spawn([&active_workers, &filenames, &cur_tex, &textures, &prefix] {
			for(;;) {
				const auto id = cur_tex++;
				if(id >= filenames.size()) break;
				
				// look for path
				string filename = filenames[id]; // check path in .mtl first
				if(!file_io::is_file(filename)) {
					filename = floor::data_path(filenames[id]); // then, check data_path root
					if(!file_io::is_file(filename)) {
						filename = prefix + filenames[id]; // finally, check with prefix
						if(!file_io::is_file(filename)) {
							log_error("couldn't find texture \"$\"!", filenames[id]);
							continue;
						}
					}
				}
				
				const auto tex = obj_loader::load_texture(filename);
				textures[id] = tex.second;
				if(!tex.first) continue;
			}
			active_workers--;
		});
	}
	while(active_workers != 0) {
		this_thread::sleep_for(10ms);
	}
#if defined(FLOOR_DEBUG)
	log_debug("$ textures loaded to mem", textures.size());
#endif
	// create metal/vulkan textures
	model_floor_textures->resize(textures.size());
	for (size_t i = 0, count = model_floor_textures->size(); i < count; ++i) {
		COMPUTE_IMAGE_TYPE image_type { COMPUTE_IMAGE_TYPE::NONE };
		
		if (textures[i].surface == nullptr && textures[i].pvrtc == nullptr) {
			log_debug("texture #$ invalid due to load failure - continuing ...", i);
			continue;
		}
		
		uint2 dim;
		void* pixels;
		size_t image_size = 0;
		if (textures[i].surface != nullptr) {
			const SDL_Surface* surface = textures[i].surface;
			image_type = obj_loader::floor_image_type_format(surface);
			dim = { uint32_t(surface->w), uint32_t(surface->h) };
			pixels = surface->pixels;
			image_size = size_t(surface->pitch) * size_t(surface->h);
		} else {
			const auto tex = textures[i].pvrtc;
			dim = tex->dim;
			pixels = tex->pixels.get();
			image_size = tex->image_size;
			
			if (tex->bpp != 2 && tex->bpp != 4) {
				log_error("invalid bpp for pvrtc texture: $", tex->bpp);
				continue;
			}
			
			if (tex->bpp == 2) {
				image_type = (tex->is_alpha ? COMPUTE_IMAGE_TYPE::PVRTC_RGBA2 : COMPUTE_IMAGE_TYPE::PVRTC_RGB2);
			} else {
				image_type = (tex->is_alpha ? COMPUTE_IMAGE_TYPE::PVRTC_RGBA4 : COMPUTE_IMAGE_TYPE::PVRTC_RGB4);
			}
		}
		
		image_type |= (COMPUTE_IMAGE_TYPE::IMAGE_2D |
					   COMPUTE_IMAGE_TYPE::READ |
					   COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED);
#if !defined(FLOOR_IOS) && defined(__APPLE__) // TODO: PVRTC+sRGB for iOS, 3ch sRGB for Vulkan
		// load diffuse and specular maps as sRGB
		if (floor::get_wide_gamut() || true) {
			if (filenames[i].find("_diff.") != string::npos ||
				filenames[i].find("_spec.") != string::npos) {
				image_type |= COMPUTE_IMAGE_TYPE::FLAG_SRGB;
			}
		}
#endif
		
		//log_debug("tex $: $, $", filenames[i], dim, compute_image::image_type_to_string(image_type));
		
		(*model_floor_textures)[i] = ctx.create_image(cqueue,
													  dim,
													  image_type,
													  span<uint8_t> { (uint8_t*)pixels, image_size },
													  COMPUTE_MEMORY_FLAG::READ |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE |
													  COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS);
		
		// assign tex ptr to tex filename
		texture_filenames[filenames[i]].second = (*model_floor_textures)[i].get();
	}
#if defined(FLOOR_DEBUG)
	log_debug("metal/vulkan/floor textures created");
#endif
	
	// cleanup
	for (auto& tex : textures) {
		if (tex.surface != nullptr) {
			SDL_DestroySurface(tex.surface);
		}
		tex.pvrtc = nullptr;
	}
	textures.clear();
}

struct obj_grammar {
	// all grammar rule objects
#if !defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_SET_NAMES)
#define FLOOR_OBJ_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__;
#else
#define FLOOR_OBJ_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__; \
	/* in debug mode, also set the debug name of each grammar rule object */ \
	void set_debug_names() { \
		string names { #__VA_ARGS__ }; \
		set_debug_name(names, __VA_ARGS__); \
	} \
	void set_debug_name(string& names, grammar_rule& obj) { \
		const auto comma_pos = names.find(","); \
		obj.debug_name = names.substr(0, comma_pos); \
		names.erase(0, comma_pos + 1 + (comma_pos+1 < names.size() && names[comma_pos+1] == ' ' ? 1 : 0)); \
	} \
	template <typename... grammar_objects> void set_debug_name(string& names, grammar_rule& obj, grammar_objects&... objects) { \
		set_debug_name(names, obj); \
		set_debug_name(names, objects...); \
	}
#endif
	
	FLOOR_OBJ_GRAMMAR_OBJECTS(global_scope, statement,
							  vertex, tex_coord, normal,
							  face_3, face_4, index_triplet1, index_triplet2, index_triplet2_vn, index_triplet3,
							  sub_object, sub_group,
							  smooth,
							  mtllib, usemtl)
	
	struct obj_face {
		uint4 vertex;
		uint4 tex_coord;
		uint4 normal;
	};
	
	struct obj_sub_object {
		const string name;
		string mat;
		vector<obj_face> indices;
	};
	
	vector<float4> vertices;
	vector<float3> tex_coords;
	vector<float3> normals;
	vector<unique_ptr<obj_sub_object>> sub_objects;
	obj_sub_object* cur_obj { nullptr };
	string mat_filename { "" };
	
	obj_grammar() {
		// fixed token type matchers:
		static constexpr literal_matcher<const char*, SOURCE_TOKEN_TYPE::INTEGER_CONSTANT> FP_CONSTANT {};
		static constexpr literal_matcher<const char*, SOURCE_TOKEN_TYPE::IDENTIFIER> IDENTIFIER {};
		
		//
		static constexpr keyword_matcher VERTEX("v"), FACE("f"), TEX_COORD("vt"), NORMAL("vn"), SUB_OBJECT("o"), SUB_GROUP("g"), MTLLIB("mtllib"), USEMTL("usemtl"), SMOOTH("s"), SMOOTH_OFF("off");
		static constexpr literal_matcher<FLOOR_PUNCTUATOR, SOURCE_TOKEN_TYPE::PUNCTUATOR> SLASH { FLOOR_PUNCTUATOR::DIV };
		
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
		set_debug_names();
#endif
		
		// grammar:
		// ref: http://www.martinreddy.net/gfx/3d/OBJ.spec
		global_scope = *statement;
		statement = (vertex | tex_coord | normal | face_4 | face_3 | sub_object | sub_group | smooth | mtllib | usemtl);
		vertex = VERTEX & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT & ~FP_CONSTANT;
		tex_coord = TEX_COORD & FP_CONSTANT & ~(FP_CONSTANT & ~FP_CONSTANT);
		normal = NORMAL & FP_CONSTANT & FP_CONSTANT & FP_CONSTANT;
		sub_object = SUB_OBJECT & IDENTIFIER;
		sub_group = SUB_GROUP & IDENTIFIER;
		smooth = SMOOTH & (FP_CONSTANT | SMOOTH_OFF);
		mtllib = MTLLIB & IDENTIFIER;
		usemtl = USEMTL & IDENTIFIER;
		
		// there are valid 4 possibilities
		// note that the spec requires that all triplets in a face have the same layout!
		// f # # #
		// f #/# #/# #/#
		// f #//# #//# #//#
		// f #/#/# #/#/# #/#/#
		index_triplet1 = FP_CONSTANT;
		index_triplet2 = FP_CONSTANT & SLASH & FP_CONSTANT;
		index_triplet2_vn = FP_CONSTANT & SLASH & SLASH & FP_CONSTANT;
		index_triplet3 = FP_CONSTANT & SLASH & FP_CONSTANT & SLASH & FP_CONSTANT;
		
		face_4 = FACE & ((index_triplet3 & index_triplet3 & index_triplet3 & index_triplet3) |
				  (index_triplet2_vn & index_triplet2_vn & index_triplet2_vn & index_triplet2_vn) |
				  (index_triplet2 & index_triplet2 & index_triplet2 & index_triplet2) |
				  (index_triplet1 & index_triplet1 & index_triplet1 & index_triplet1));
		
		face_3 = FACE & ((index_triplet3 & index_triplet3 & index_triplet3) |
				  (index_triplet2_vn & index_triplet2_vn & index_triplet2_vn) |
				  (index_triplet2 & index_triplet2 & index_triplet2) |
				  (index_triplet1 & index_triplet1 & index_triplet1));
		
		// match handling
		static const auto match_to_float = [](const parser_context::match& match) {
			return stof(match.token->second.to_string());
		};
		static const auto match_to_uint = [](const parser_context::match& match) {
			return stou(match.token->second.to_string());
		};
		static const auto push_to_parent = [](parser_context::match_list& matches) -> parser_context::match_list {
			return std::move(matches);
		};
		
		vertex.on_match([this](auto& matches) -> parser_context::match_list {
			vertices.emplace_back(match_to_float(matches[1]),
								  match_to_float(matches[2]),
								  match_to_float(matches[3]),
								  (matches.size() > 4 ? match_to_float(matches[4]) : 1.0f));
			return {};
		});
		tex_coord.on_match([this](auto& matches) -> parser_context::match_list {
			tex_coords.emplace_back(match_to_float(matches[1]),
									// always flip y ...
									(matches.size() > 2 ? 1.0f - match_to_float(matches[2]) : 0.0f),
									(matches.size() > 3 ? match_to_float(matches[3]) : 0.0f));
			return {};
		});
		normal.on_match([this](auto& matches) -> parser_context::match_list {
			normals.emplace_back(match_to_float(matches[1]),
								 match_to_float(matches[2]),
								 match_to_float(matches[3]));
			return {};
		});
		face_3.on_match([this](parser_context::match_list& matches) -> parser_context::match_list {
			const size_t count = matches.size();
			size_t type = 0;
			switch(count) {
				case 4: type = 0; break;
				case 10: type = 1; break;
				case 13: type = 2; break;
				case 16: type = 3; break;
				default:
					log_error("face_3: invalid match count: $", count);
					return {};
			}
			
			static constexpr const array<uint3, 4> v_offsets {{
				{ 1, 2, 3 },
				{ 1, 4, 7 },
				{ 1, 5, 9 },
				{ 1, 6, 11 },
			}};
			static constexpr const array<uint3, 4> tc_offsets {{
				{ 0, 0, 0 },
				{ 3, 6, 9 },
				{ 0, 0, 0 },
				{ 3, 8, 13 },
			}};
			static constexpr const array<uint3, 4> n_offsets {{
				{ 0, 0, 0 },
				{ 0, 0, 0 },
				{ 4, 8, 12 },
				{ 5, 10, 15 },
			}};
			
			const auto& v_offset = v_offsets[type];
			const auto& tc_offset = tc_offsets[type];
			const auto& n_offset = n_offsets[type];
			
			cur_obj->indices.emplace_back(obj_face {
				uint4 {
					match_to_uint(matches[v_offset.x]),
					match_to_uint(matches[v_offset.y]),
					match_to_uint(matches[v_offset.z]),
					0
				},
				uint4 {
					tc_offset.x == 0 ? 0 : match_to_uint(matches[tc_offset.x]),
					tc_offset.y == 0 ? 0 : match_to_uint(matches[tc_offset.y]),
					tc_offset.z == 0 ? 0 : match_to_uint(matches[tc_offset.z]),
					0
				},
				uint4 {
					n_offset.x == 0 ? 0 : match_to_uint(matches[n_offset.x]),
					n_offset.y == 0 ? 0 : match_to_uint(matches[n_offset.y]),
					n_offset.z == 0 ? 0 : match_to_uint(matches[n_offset.z]),
					0
				}
			});
			return {};
		});
		face_4.on_match([this](auto& matches) -> parser_context::match_list {
			const size_t count = matches.size();
			size_t type = 0;
			switch(count) {
				case 5: type = 0; break;
				case 13: type = 1; break;
				case 17: type = 2; break;
				case 21: type = 3; break;
				default:
					log_error("face_4: invalid match count: $", count);
					return {};
			}
			
			static constexpr const array<uint4, 4> v_offsets {{
				{ 1, 2, 3, 4 },
				{ 1, 4, 7, 10 },
				{ 1, 5, 9, 13 },
				{ 1, 6, 11, 16 },
			}};
			static constexpr const array<uint4, 4> tc_offsets {{
				{ 0, 0, 0, 0 },
				{ 3, 6, 9, 12 },
				{ 0, 0, 0, 0 },
				{ 3, 8, 13, 18 },
			}};
			static constexpr const array<uint4, 4> n_offsets {{
				{ 0, 0, 0, 0 },
				{ 0, 0, 0, 0 },
				{ 4, 8, 12, 16 },
				{ 5, 10, 15, 20 },
			}};
			
			const auto& v_offset = v_offsets[type];
			const auto& tc_offset = tc_offsets[type];
			const auto& n_offset = n_offsets[type];
			
			cur_obj->indices.emplace_back(obj_face {
				uint4 {
					match_to_uint(matches[v_offset.x]),
					match_to_uint(matches[v_offset.y]),
					match_to_uint(matches[v_offset.z]),
					match_to_uint(matches[v_offset.w])
				},
				uint4 {
					tc_offset.x == 0 ? 0 : match_to_uint(matches[tc_offset.x]),
					tc_offset.y == 0 ? 0 : match_to_uint(matches[tc_offset.y]),
					tc_offset.z == 0 ? 0 : match_to_uint(matches[tc_offset.z]),
					tc_offset.w == 0 ? 0 : match_to_uint(matches[tc_offset.w])
				},
				uint4 {
					n_offset.x == 0 ? 0 : match_to_uint(matches[n_offset.x]),
					n_offset.y == 0 ? 0 : match_to_uint(matches[n_offset.y]),
					n_offset.z == 0 ? 0 : match_to_uint(matches[n_offset.z]),
					n_offset.w == 0 ? 0 : match_to_uint(matches[n_offset.w])
				}
			});
			return {};
		});
		index_triplet1.on_match(push_to_parent);
		index_triplet2.on_match(push_to_parent);
		index_triplet2_vn.on_match(push_to_parent);
		index_triplet3.on_match(push_to_parent);
		sub_group.on_match([this](auto& matches) -> parser_context::match_list {
			auto obj = make_unique<obj_sub_object>(obj_sub_object { .name = matches[1].token->second.to_string() });
			sub_objects.emplace_back(std::move(obj));
			cur_obj = sub_objects.back().get();
			return {};
		});
		sub_object.on_match([](auto& matches floor_unused) -> parser_context::match_list {
			return {}; // ignore
		});
		mtllib.on_match([this](auto& matches) -> parser_context::match_list {
#if !defined(FLOOR_IOS)
			mat_filename = matches[1].token->second.to_string();
#else
			mat_filename = "ios_" + matches[1].token->second.to_string();
#endif
			return {};
		});
		usemtl.on_match([this](auto& matches) -> parser_context::match_list {
			// if a material has already been assigned, a new sub-object must be created
			if(cur_obj->mat != "") {
				auto obj = make_unique<obj_sub_object>(obj_sub_object { .name = cur_obj->name + "#" });
				sub_objects.emplace_back(std::move(obj));
				cur_obj = sub_objects.back().get();
			}
			cur_obj->mat = matches[1].token->second.to_string();
			return {};
		});
	}
	
	void parse(parser_context& ctx, bool& success, const bool is_load_textures, shared_ptr<obj_model> model,
			   const compute_context& comp_ctx, const compute_queue& cqueue) {
		// clear all
		vertices.clear();
		tex_coords.clear();
		normals.clear();
		sub_objects.clear();
		cur_obj = nullptr;
		mat_filename = "";
		success = false;
		
		// parse
		global_scope.match(ctx);
		
		// if the end hasn't been reached, we have an error
		if(ctx.iter != ctx.end) {
			string error_msg = "parsing failed: ";
			if(ctx.deepest_iter == ctx.tu.tokens.cend()) {
				error_msg += "premature EOF after";
				ctx.deepest_iter = ctx.end - 1; // set iter to token before EOF
			}
			else {
				error_msg += "possibly at";
			}
			error_msg += " \"" + ctx.deepest_iter->second.to_string() + "\"";
			
			const auto line_and_column = obj_lexer::get_line_and_column_from_iter(ctx.tu, ctx.deepest_iter->second.begin);
			log_error("$:$:$: $",
					  ctx.tu.file_name, line_and_column.first, line_and_column.second, error_msg);
			return;
		}
#if defined(FLOOR_DEBUG)
		log_debug("parsing done");
#endif
		
		// create proper model
		model->vertices.reserve(vertices.size());
		model->tex_coords.reserve(vertices.size());
		model->normals.reserve(vertices.size());
		model->binormals.reserve(vertices.size());
		model->tangents.reserve(vertices.size());
		model->materials_data.reserve(vertices.size());
		model->objects.reserve(sub_objects.size());
#if defined(FLOOR_DEBUG)
		log_debug("alloc #0");
#endif
		
		struct reassign_entry {
			struct target {
				uint32_t tc_idx { 0 };
				uint32_t n_idx { 0 };
				uint32_t dst_idx { 0 };
			};
			uint32_t used { 0 };
			vector<target> targets;
			
			// start off with 4 everywhere (should be enough for most models, otherwise huge realloc will ensue ...)
			reassign_entry() : targets(4) {}
			
			void add(const uint32_t& tc, const uint32_t& n, const uint32_t& dst) {
				if(used < targets.size()) {
					targets[used].tc_idx = tc;
					targets[used].n_idx = n;
					targets[used].dst_idx = dst;
					++used;
					return;
				}
				log_error("meh: $: $ $", used, tc, n);
				targets.emplace_back(target { tc, n, dst });
				++used;
			}
		};
		vector<reassign_entry> index_reassign(vertices.size() + 1);
#if defined(FLOOR_DEBUG)
		log_debug("alloc #1");
#endif
		
		// if a model doesn't contain any texture coordinates, add at least one dummy/default one
		if(tex_coords.empty()) {
			tex_coords.emplace_back(float2 {});
		}
		
		uint32_t index_counter = 1u; // start off at 1, 0 is the "invalid"/zero data
		const auto reassign = [this, &model, &index_reassign, &index_counter](const uint32_t& v_idx,
																			  const uint32_t& tc_idx,
																			  const uint32_t& n_idx) {
			auto& entry = index_reassign[v_idx];
			for(uint32_t i = 0; i < entry.used; ++i) {
				if(entry.targets[i].tc_idx == tc_idx /*&&
				   entry.targets[i].n_idx == n_idx*/) {
					return entry.targets[i].dst_idx;
				}
			}
			
			// not found, create a new target
			const uint32_t dst = index_counter++;
			model->vertices.push_back(vertices[v_idx > 0 ? v_idx - 1 : v_idx].xyz);
			model->tex_coords.push_back(tex_coords[tc_idx > 0 ? tc_idx - 1 : tc_idx].xy);
			model->normals.push_back(float3 {});
			model->binormals.push_back(float3 {});
			model->tangents.push_back(float3 {});
			model->materials_data.push_back(0u);
			entry.add(tc_idx, n_idx, dst);
			return dst;
		};
		
		//
		for(const auto& obj : sub_objects) {
			auto sobj = make_unique<obj_model::sub_object>(obj_model::sub_object { .name = obj->name });
			
			sobj->indices.reserve(obj->indices.size());
			for(const auto& triplet : obj->indices) {
				// .obj has the issue that it handles vertex, tex-coord and normal indices separately instead of using a combined index
				// -> map from the triplet to a new index
				
				// 3-triplet / triangle
				if(triplet.vertex.w == 0) {
					sobj->indices.emplace_back(uint3 {
						reassign(triplet.vertex.x, triplet.tex_coord.x, triplet.normal.x),
						reassign(triplet.vertex.y, triplet.tex_coord.y, triplet.normal.y),
						reassign(triplet.vertex.z, triplet.tex_coord.z, triplet.normal.z)
					});
				}
				// 4-triplet / quad
				else {
					const uint4 reassigned_indices {
						reassign(triplet.vertex.x, triplet.tex_coord.x, triplet.normal.x),
						reassign(triplet.vertex.y, triplet.tex_coord.y, triplet.normal.y),
						reassign(triplet.vertex.z, triplet.tex_coord.z, triplet.normal.z),
						reassign(triplet.vertex.w, triplet.tex_coord.w, triplet.normal.w)
					};
					sobj->indices.emplace_back(uint3 { reassigned_indices.x, reassigned_indices.y, reassigned_indices.z });
					sobj->indices.emplace_back(uint3 { reassigned_indices.x, reassigned_indices.z, reassigned_indices.w });
				}
			}
			
			model->objects.emplace_back(std::move(sobj));
		}
#if defined(FLOOR_DEBUG)
		log_debug("objects done");
		log_debug("indices: $ -> $", vertices.size(), index_counter);
#endif
		
		//
		for(const auto& obj : model->objects) {
			for(const auto& face : obj->indices) {
				const auto delta_1 = model->tex_coords[face.y] - model->tex_coords[face.x];
				const auto delta_2 = model->tex_coords[face.z] - model->tex_coords[face.x];
				const float3 edge1 = model->vertices[face.y] - model->vertices[face.x];
				const float3 edge2 = model->vertices[face.z] - model->vertices[face.x];
				
				float3 norm = edge1.crossed(edge2);
				float3 binormal = (edge1 * delta_2.x) - (edge2 * delta_1.x);
				float3 tangent = (edge1 * delta_2.y) - (edge2 * delta_1.y);
				
				// adjust
				if(norm.dot(tangent.crossed(binormal)) > 0.0f) {
					tangent *= -1.0f;
				}
				else binormal *= -1.0f;
				
				model->normals[face.x] += norm;
				model->normals[face.y] += norm;
				model->normals[face.z] += norm;
				model->binormals[face.x] += binormal;
				model->binormals[face.y] += binormal;
				model->binormals[face.z] += binormal;
				model->tangents[face.x] += tangent;
				model->tangents[face.y] += tangent;
				model->tangents[face.z] += tangent;
			}
		}
		
		for(auto& norm : model->normals) { norm.normalize(); }
		for(auto& binormal : model->binormals) { binormal.normalize(); }
		for(auto& tangent : model->tangents) { tangent.normalize(); }
		
		// process material
		if(mat_filename != "") {
			string prefix = "";
			const auto slash_pos = ctx.tu.file_name.rfind('/');
			if(slash_pos != string::npos) {
				prefix = ctx.tu.file_name.substr(0, slash_pos + 1);
			}
#if defined(FLOOR_DEBUG)
			log_debug("loading mat: $", mat_filename);
#endif
			string mat_data = "";
			if(!file_io::file_to_string(prefix + mat_filename, mat_data)) {
				log_error("failed to load .mtl file: $", prefix + mat_filename);
				return;
			}
			
			auto mat_tu = make_unique<translation_unit>(mat_filename);
			mat_tu->source.insert(0, mat_data.c_str(), mat_data.size());
			
			lexer::map_characters(*mat_tu);
			obj_lexer::lex(*mat_tu);
			obj_lexer::assign_token_sub_types(*mat_tu);
#if defined(FLOOR_DEBUG)
			log_debug("mat lexed");
#endif
			
			parser_context mtl_parser_ctx { *mat_tu };
			mtl_grammar mtl_grammar_parser;
			auto mats = mtl_grammar_parser.parse(mtl_parser_ctx, success);
			if(!success) {
				log_error("mtl parsing failed");
				return;
			}
			success = false; // reset for further processing
#if defined(FLOOR_DEBUG)
			log_debug("mtl parsed");
#endif
			
			// create a map of all texture file names
			// value: at first use count, later: gl tex id
			unordered_map<string, pair<uint32_t, compute_image*>> texture_filenames;
			for(const auto& mat : mats) {
				texture_filenames.emplace(mat.second->ambient, pair<uint32_t, compute_image*> { 0, nullptr });
				texture_filenames.emplace(mat.second->diffuse, pair<uint32_t, compute_image*> { 0, nullptr });
				texture_filenames.emplace(mat.second->specular, pair<uint32_t, compute_image*> { 0, nullptr });
				texture_filenames.emplace(mat.second->normal, pair<uint32_t, compute_image*> { 0, nullptr });
				texture_filenames.emplace(mat.second->mask, pair<uint32_t, compute_image*> { 0, nullptr });
				texture_filenames.emplace(mat.second->displacement, pair<uint32_t, compute_image*> { 0, nullptr });
			}
			
			// check use count
			for(const auto& obj : sub_objects) {
				const auto mat_iter = mats.find(obj->mat);
				if(mat_iter == mats.end()) {
					log_error("object \"$\" references an unknown material \"$\"!",
							  obj->name, obj->mat);
					continue;
				}
				++texture_filenames[mat_iter->second->ambient].first;
				++texture_filenames[mat_iter->second->diffuse].first;
				++texture_filenames[mat_iter->second->specular].first;
				++texture_filenames[mat_iter->second->normal].first;
				++texture_filenames[mat_iter->second->mask].first;
				++texture_filenames[mat_iter->second->displacement].first;
			}
			
			// kill empty string
			const auto empty_iter = texture_filenames.find("");
			if(empty_iter != texture_filenames.end()) texture_filenames.erase(empty_iter);
			
			// kill unused
			core::erase_if(texture_filenames, [](const auto& iter) -> bool {
				if(iter->second.first == 0) {
#if defined(FLOOR_DEBUG)
					log_debug("killing unused texture \"$\"", iter->first);
#endif
				}
				return (iter->second.first == 0);
			});
			
			// add dummy textures for later
			texture_filenames.emplace("black.png", pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace("gray.png", pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace("white.png", pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace("up_normal.png", pair<uint32_t, compute_image*> { 0, nullptr });
			
			//
			floor_obj_model* floor_model = (floor_obj_model*)model.get();
			floor_model->diffuse_textures.resize(mats.size());
			floor_model->specular_textures.resize(mats.size());
			floor_model->normal_textures.resize(mats.size());
			floor_model->mask_textures.resize(mats.size());
			floor_model->displacement_textures.resize(mats.size());
			model->material_infos.resize(mats.size());
			if (is_load_textures) {
				load_textures(texture_filenames,
							  &floor_model->textures,
							  prefix, comp_ctx, cqueue);
			}
			
			// assign textures/materials
			unordered_map<string, uint32_t> mat_map; // mat name -> mat idx in model
			uint32_t mat_counter { 0u };
			for(const auto& mat : mats) {
				mat_map[mat.first] = mat_counter++;
			}
			for(const auto& mat : mats) {
				auto& mdl_mat_info = model->material_infos[mat_map[mat.first]];
				
				mdl_mat_info.name = mat.first;
				mdl_mat_info.diffuse_file_name = (mat.second->diffuse != "" ? mat.second->diffuse : "black.png");
				mdl_mat_info.specular_file_name = (mat.second->specular != "" ? mat.second->specular : "black.png");
				mdl_mat_info.normal_file_name = (mat.second->normal != "" ? mat.second->normal : "up_normal.png");
				mdl_mat_info.mask_file_name = (mat.second->mask != "" ? mat.second->mask : "white.png");
				mdl_mat_info.displacement_file_name = (mat.second->displacement != "" ? mat.second->displacement : "gray.png");
				mdl_mat_info.has_proper_displacement = (mat.second->displacement != "" ? 1u : 0u);
				
				floor_model->diffuse_textures[mat_map[mat.first]] = texture_filenames[mdl_mat_info.diffuse_file_name].second;
				floor_model->specular_textures[mat_map[mat.first]] = texture_filenames[mdl_mat_info.specular_file_name].second;
				floor_model->normal_textures[mat_map[mat.first]] = texture_filenames[mdl_mat_info.normal_file_name].second;
				floor_model->mask_textures[mat_map[mat.first]] = texture_filenames[mdl_mat_info.mask_file_name].second;
				floor_model->displacement_textures[mat_map[mat.first]] = texture_filenames[mdl_mat_info.displacement_file_name].second;
			}
			
			// assign materials to sub-objects
			for(size_t i = 0, count = sub_objects.size(); i < count; ++i) {
				model->objects[i]->mat_idx = mat_map[sub_objects[i]->mat];
			}
#if defined(FLOOR_DEBUG)
			log_debug("materials assigned");
#endif
		}
		
		// done
		success = true;
	}
	
};

shared_ptr<obj_model> obj_loader::load(const string& file_name, bool& success,
									   const compute_context& ctx,
									   const compute_queue& cqueue,
									   const float scale,
									   const bool cleanup_cpu_data,
									   const bool is_load_textures,
									   const bool create_gpu_buffers,
									   const COMPUTE_MEMORY_FLAG add_mem_flags) {
	success = false;
	
	shared_ptr<floor_obj_model> floor_model = make_shared<floor_obj_model>();
	shared_ptr<obj_model> model = (shared_ptr<obj_model>)floor_model;
	static constexpr const uint32_t bin_obj_version { 1 };
	
	// -> load .obj
	if(!file_io::is_file(file_name + ".bin")) {
		string obj_data = "";
		if(!file_io::file_to_string(file_name, obj_data)) {
			log_error("failed to load .obj file: $", file_name);
			return {};
		}
#if defined(FLOOR_DEBUG)
		log_debug("loaded");
#endif
		
		auto tu = make_unique<translation_unit>(file_name);
		tu->source.insert(0, obj_data.c_str(), obj_data.size());
		
		obj_lexer::map_characters(*tu);
		obj_lexer::lex(*tu);
		obj_lexer::assign_token_sub_types(*tu);
#if defined(FLOOR_DEBUG)
		log_debug("obj lexed");
#endif
		
		parser_context parser_ctx { *tu };
		obj_grammar obj_grammar_parser;
		obj_grammar_parser.parse(parser_ctx, success, is_load_textures, model, ctx, cqueue);
		if(!success) {
			log_error("obj parsing failed");
			return {};
		}
#if defined(FLOOR_DEBUG)
		log_debug("obj parsed");
#endif
		
		// scale vertices
		for(auto& vertex : model->vertices) {
			vertex *= scale;
		}
		
		// write .bin
#if !defined(FLOOR_IOS)
		file_io bin_file(file_name + ".bin", file_io::OPEN_TYPE::WRITE_BINARY);
#else
		string bin_file_name = file_name;
		const auto slash_pos = file_name.rfind('/');
		if(slash_pos != string::npos) {
			bin_file_name = file_name.substr(slash_pos + 1);
		}
		file_io bin_file(darwin_helper::get_pref_path() + bin_file_name + ".bin", file_io::OPEN_TYPE::WRITE_BINARY);
#endif
		if(!bin_file.is_open()) {
			log_warn("failed to open .bin file!");
		}
		else {
			bin_file.write_uint(bin_obj_version);
			bin_file.write_uint((uint32_t)model->vertices.size());
			bin_file.write_uint((uint32_t)model->objects.size());
			bin_file.write_uint((uint32_t)floor_model->diffuse_textures.size());
			
			for(const auto& mtl : model->material_infos) {
				bin_file.write_terminated_block(mtl.name, 0);
				bin_file.write_terminated_block(mtl.diffuse_file_name, 0);
				bin_file.write_terminated_block(mtl.specular_file_name, 0);
				bin_file.write_terminated_block(mtl.normal_file_name, 0);
				bin_file.write_terminated_block(mtl.mask_file_name, 0);
				bin_file.write_terminated_block(mtl.displacement_file_name, 0);
			}
			
			for(const auto& obj : model->objects) {
				bin_file.write_terminated_block(obj->name, 0);
				bin_file.write_uint((uint32_t)obj->indices.size());
				bin_file.write_uint(obj->mat_idx);
				for(const auto& idx : obj->indices) {
					bin_file.write_uint(idx.x);
					bin_file.write_uint(idx.y);
					bin_file.write_uint(idx.z);
				}
			}
			
			for(const auto& vtx : model->vertices) {
				bin_file.write_float(vtx.x);
				bin_file.write_float(vtx.y);
				bin_file.write_float(vtx.z);
			}
			for(const auto& tc : model->tex_coords) {
				bin_file.write_float(tc.x);
				bin_file.write_float(tc.y);
			}
			for(const auto& nrm : model->normals) {
				bin_file.write_float(nrm.x);
				bin_file.write_float(nrm.y);
				bin_file.write_float(nrm.z);
			}
			for(const auto& bnm : model->binormals) {
				bin_file.write_float(bnm.x);
				bin_file.write_float(bnm.y);
				bin_file.write_float(bnm.z);
			}
			for(const auto& tgt : model->tangents) {
				bin_file.write_float(tgt.x);
				bin_file.write_float(tgt.y);
				bin_file.write_float(tgt.z);
			}
		}
	}
	// -> load .obj.bin
	else {
#if defined(FLOOR_DEBUG)
		log_debug("loading bin ...");
#endif
		
		file_io bin_file(file_name + ".bin", file_io::OPEN_TYPE::READ_BINARY);
		if(!bin_file.is_open()) {
			log_error("failed to open .bin file!");
			return {};
		}
		
		string prefix = "";
		const auto slash_pos = file_name.rfind('/');
		if(slash_pos != string::npos) {
			prefix = file_name.substr(0, slash_pos + 1);
		}
		
		//
		if(bin_file.get_uint() != bin_obj_version) {
			log_error("version mismatch - expected $!", bin_obj_version);
		}
		
		const auto vertex_count = bin_file.get_uint();
		const auto sub_object_count = bin_file.get_uint();
		const auto mat_count = bin_file.get_uint();
		
		model->vertices.clear();
		model->tex_coords.clear();
		model->normals.clear();
		model->binormals.clear();
		model->tangents.clear();
		model->vertices.reserve(vertex_count);
		model->tex_coords.reserve(vertex_count);
		model->normals.reserve(vertex_count);
		model->binormals.reserve(vertex_count);
		model->tangents.reserve(vertex_count);
		floor_model->diffuse_textures.reserve(mat_count);
		floor_model->specular_textures.reserve(mat_count);
		floor_model->normal_textures.reserve(mat_count);
		floor_model->mask_textures.reserve(mat_count);
		floor_model->displacement_textures.reserve(mat_count);
		model->material_infos.reserve(mat_count);
		model->objects.reserve(sub_object_count);
		
		for(uint32_t i = 0; i < mat_count; ++i) {
			obj_model::material_info mat_info;
			mat_info.name = bin_file.get_terminated_block(0);
			mat_info.diffuse_file_name = bin_file.get_terminated_block(0);
			mat_info.specular_file_name = bin_file.get_terminated_block(0);
			mat_info.normal_file_name = bin_file.get_terminated_block(0);
			mat_info.mask_file_name = bin_file.get_terminated_block(0);
			mat_info.displacement_file_name = bin_file.get_terminated_block(0);
			mat_info.has_proper_displacement = (mat_info.displacement_file_name.find("gray.png") == string::npos ? 1u : 0u);
			model->material_infos.emplace_back(mat_info);
		}
		
		unordered_map<string, pair<uint32_t, compute_image*>> texture_filenames;
		for(const auto& mat : model->material_infos) {
			texture_filenames.emplace(mat.diffuse_file_name, pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace(mat.specular_file_name, pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace(mat.normal_file_name, pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace(mat.mask_file_name, pair<uint32_t, compute_image*> { 0, nullptr });
			texture_filenames.emplace(mat.displacement_file_name, pair<uint32_t, compute_image*> { 0, nullptr });
		}
		if (is_load_textures) {
			load_textures(texture_filenames,
						  &floor_model->textures,
						  prefix, ctx, cqueue);
		}
		
		for(uint32_t i = 0; i < mat_count; ++i) {
			auto& mdl_mat_info = model->material_infos[i];
			floor_model->diffuse_textures.emplace_back(texture_filenames[mdl_mat_info.diffuse_file_name].second);
			floor_model->specular_textures.emplace_back(texture_filenames[mdl_mat_info.specular_file_name].second);
			floor_model->normal_textures.emplace_back(texture_filenames[mdl_mat_info.normal_file_name].second);
			floor_model->mask_textures.emplace_back(texture_filenames[mdl_mat_info.mask_file_name].second);
			floor_model->displacement_textures.emplace_back(texture_filenames[mdl_mat_info.displacement_file_name].second);
		}
		
		for(uint32_t i = 0; i < sub_object_count; ++i) {
			auto obj = make_unique<obj_model::sub_object>(obj_model::sub_object {
				.name = bin_file.get_terminated_block(0)
			});
			const auto index_count = bin_file.get_uint();
			obj->indices.reserve(index_count);
			
			obj->mat_idx = bin_file.get_uint();
			
			for(uint32_t j = 0; j < index_count; ++j) {
				uint3 idx;
				idx.x = bin_file.get_uint();
				idx.y = bin_file.get_uint();
				idx.z = bin_file.get_uint();
				obj->indices.emplace_back(idx);
			}
			
			model->objects.emplace_back(std::move(obj));
		}
#if defined(FLOOR_DEBUG)
		log_debug("loaded sub-objects");
#endif
		
		for(uint32_t i = 0; i < vertex_count; ++i) {
			float3 vtx;
			vtx.x = bin_file.get_float();
			vtx.y = bin_file.get_float();
			vtx.z = bin_file.get_float();
			model->vertices.emplace_back(vtx);
		}
		for(uint32_t i = 0; i < vertex_count; ++i) {
			float2 tc;
			tc.x = bin_file.get_float();
			tc.y = bin_file.get_float();
			model->tex_coords.emplace_back(tc);
		}
		for(uint32_t i = 0; i < vertex_count; ++i) {
			float3 norm;
			norm.x = bin_file.get_float();
			norm.y = bin_file.get_float();
			norm.z = bin_file.get_float();
			model->normals.emplace_back(norm);
		}
		for(uint32_t i = 0; i < vertex_count; ++i) {
			float3 bn;
			bn.x = bin_file.get_float();
			bn.y = bin_file.get_float();
			bn.z = bin_file.get_float();
			model->binormals.emplace_back(bn);
		}
		for(uint32_t i = 0; i < vertex_count; ++i) {
			float3 tn;
			tn.x = bin_file.get_float();
			tn.y = bin_file.get_float();
			tn.z = bin_file.get_float();
			model->tangents.emplace_back(tn);
		}
		
		success = true;
#if defined(FLOOR_DEBUG)
		log_debug("loaded model data");
#endif
	}
	
	// create model buffers
	for(auto& obj : floor_model->objects) {
		obj->index_count = (uint32_t)(obj->indices.size() * 3);
	}
	if(create_gpu_buffers) {
		// set material indices (only needed for floor_obj_model)
		model->materials_data.clear();
		model->materials_data.resize(floor_model->vertices.size());
		for (const auto& obj : floor_model->objects) {
			for (const auto& index : obj->indices) {
				floor_model->materials_data[index.x] = obj->mat_idx | (model->material_infos[obj->mat_idx].has_proper_displacement ? 0x10000u : 0u);
				floor_model->materials_data[index.y] = obj->mat_idx | (model->material_infos[obj->mat_idx].has_proper_displacement ? 0x10000u : 0u);
				floor_model->materials_data[index.z] = obj->mat_idx | (model->material_infos[obj->mat_idx].has_proper_displacement ? 0x10000u : 0u);
			}
		}
		
		// create buffers
		const auto buffer_type = (COMPUTE_MEMORY_FLAG::READ |
								  COMPUTE_MEMORY_FLAG::HOST_WRITE |
								  add_mem_flags);
		floor_model->vertices_buffer = ctx.create_buffer(cqueue, floor_model->vertices, buffer_type);
		floor_model->tex_coords_buffer = ctx.create_buffer(cqueue, floor_model->tex_coords, buffer_type);
		floor_model->normals_buffer = ctx.create_buffer(cqueue, floor_model->normals, buffer_type);
		floor_model->binormals_buffer = ctx.create_buffer(cqueue, floor_model->binormals, buffer_type);
		floor_model->tangents_buffer = ctx.create_buffer(cqueue, floor_model->tangents, buffer_type);
		floor_model->materials_data_buffer = ctx.create_buffer(cqueue, floor_model->materials_data, buffer_type);
		
		vector<uint3> all_indices;
		for (auto& obj : floor_model->objects) {
			obj->indices_floor_vbo = ctx.create_buffer(cqueue, obj->indices, buffer_type);
			all_indices.insert(end(all_indices), begin(obj->indices), end(obj->indices));
		}
		floor_model->index_count = (uint32_t)(all_indices.size() * 3);
		floor_model->indices_buffer = ctx.create_buffer(cqueue, all_indices, buffer_type);
		
#if defined(FLOOR_DEBUG)
		auto model_name = file_name;
		if (const auto last_slash = model_name.rfind('/'); last_slash != string::npos) {
			model_name = model_name.substr(last_slash + 1);
		}
		if (const auto last_dot = model_name.rfind('.'); last_dot != string::npos) {
			model_name = model_name.substr(0, last_dot);
		}
		floor_model->vertices_buffer->set_debug_label(model_name + ":vertices");
		floor_model->tex_coords_buffer->set_debug_label(model_name + ":tex_coords");
		floor_model->normals_buffer->set_debug_label(model_name + ":normals");
		floor_model->binormals_buffer->set_debug_label(model_name + ":binormals");
		floor_model->tangents_buffer->set_debug_label(model_name + ":tangents");
		floor_model->materials_data_buffer->set_debug_label(model_name + ":materials_data");
		for (auto& obj : floor_model->objects) {
			obj->indices_floor_vbo->set_debug_label(model_name + ":sub_obj_indices");
		}
		floor_model->indices_buffer->set_debug_label(model_name + ":all_indices");
#endif
	}
	
	// clean up mem that isn't needed any more
	if(cleanup_cpu_data) {
		model->vertices.clear();
		model->tex_coords.clear();
		model->normals.clear();
		model->binormals.clear();
		model->tangents.clear();
		model->materials_data.clear();
		model->material_infos.clear();
		for(auto& obj : model->objects) {
			obj->indices.clear();
		}
	}
	
	return model;
}
