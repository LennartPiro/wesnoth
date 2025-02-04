/*
	Copyright (C) 2007 - 2022
	by Mark de Wever <koraq@xs4all.nl>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

/**
 * @file
 * Implementation of canvas.hpp.
 */

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/core/canvas.hpp"
#include "gui/core/canvas_private.hpp"

#include "draw.hpp"
#include "font/text.hpp"
#include "formatter.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/typed_formula.hpp"
#include "gui/core/log.hpp"
#include "gui/widgets/helper.hpp"
#include "picture.hpp"
#include "sdl/point.hpp"
#include "sdl/rect.hpp"
#include "sdl/texture.hpp"
#include "video.hpp"
#include "wml_exception.hpp"

namespace gui2
{

/***** ***** ***** ***** ***** LINE ***** ***** ***** ***** *****/

line_shape::line_shape(const config& cfg)
	: shape(cfg)
	, x1_(cfg["x1"])
	, y1_(cfg["y1"])
	, x2_(cfg["x2"])
	, y2_(cfg["y2"])
	, color_(cfg["color"])
	, thickness_(cfg["thickness"])
{
	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_GUI_P << "Line: found debug message '" << debug << "'.\n";
	}
}

void line_shape::draw(
	const SDL_Rect& portion_to_draw,
	const SDL_Rect& draw_location,
	wfl::map_formula_callable& variables)
{
	/**
	 * @todo formulas are now recalculated every draw cycle which is a bit silly
	 * unless there has been a resize. So to optimize we should use an extra
	 * flag or do the calculation in a separate routine.
	 */

	const unsigned x1 = draw_location.x + x1_(variables) - portion_to_draw.x;
	const unsigned y1 = draw_location.y + y1_(variables) - portion_to_draw.y;
	const unsigned x2 = draw_location.x + x2_(variables) - portion_to_draw.x;
	const unsigned y2 = draw_location.y + y2_(variables) - portion_to_draw.y;

	DBG_GUI_D << "Line: draw from " << x1 << ',' << y1 << " to " << x2 << ',' << y2
			  << " within bounds {" << portion_to_draw.x << ", " << portion_to_draw.y
			  << ", " << portion_to_draw.w << ", " << portion_to_draw.h << "}.\n";

	// @todo FIXME respect the thickness.

	draw::line(x1, y1, x2, y2, color_(variables));
}

/***** ***** ***** Base class for rectangular shapes ***** ***** *****/

rect_bounded_shape::rect_bounded_shape(const config& cfg)
	: shape(cfg)
	, x_(cfg["x"])
	, y_(cfg["y"])
	, w_(cfg["w"])
	, h_(cfg["h"])
{
}

rect_bounded_shape::calculated_rects rect_bounded_shape::calculate_rects(const SDL_Rect& view_bounds, wfl::map_formula_callable& variables) const
{
	// Formulas are recalculated every draw cycle, even if there hasn't been a resize.

	const unsigned x = x_(variables);
	const unsigned y = y_(variables);
	const unsigned w = w_(variables);
	const unsigned h = h_(variables);

	const auto dst_on_widget = sdl::create_rect(x, y, w, h);

	SDL_Rect clip_on_widget;
	if(!SDL_IntersectRect(&dst_on_widget, &view_bounds, &clip_on_widget)) {
		DBG_GUI_D << "Text: Clipping view_bounds resulted in an empty intersection, nothing to do.\n";
		return {true, dst_on_widget, {}, {}, {}, {}};
	}

	auto unclipped_around_viewport = dst_on_widget;
	unclipped_around_viewport.x -= view_bounds.x;
	unclipped_around_viewport.y -= view_bounds.y;

	auto clip_in_shape = clip_on_widget;
	clip_in_shape.x -= x;
	clip_in_shape.y -= y;

	auto dst_in_viewport = clip_on_widget;
	dst_in_viewport.x -= view_bounds.x;
	dst_in_viewport.y -= view_bounds.y;

	DBG_GUI_D << "Calculate_rects: from " << x << ',' << y << " width " << w << " height " << h << "\n"
			  << " view_bounds {" << view_bounds.x << ", " << view_bounds.y << ", "
			  << view_bounds.w << ", " << view_bounds.h << "}.\n"
			  << " dst_in_viewport {" << dst_in_viewport.x << ", " << dst_in_viewport.y << ", "
			  << dst_in_viewport.w << ", " << dst_in_viewport.h << "}.\n";

	return {false, dst_on_widget, clip_on_widget, clip_in_shape, unclipped_around_viewport, dst_in_viewport};
}

/***** ***** ***** ***** ***** Rectangle ***** ***** ***** ***** *****/

rectangle_shape::rectangle_shape(const config& cfg)
	: rect_bounded_shape(cfg)
	, border_thickness_(cfg["border_thickness"])
	, border_color_(cfg["border_color"], color_t::null_color())
	, fill_color_(cfg["fill_color"], color_t::null_color())
{
	// Check if a raw color string evaluates to a null color.
	if(!border_color_.has_formula() && border_color_().null()) {
		border_thickness_ = 0;
	}

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_GUI_P << "Rectangle: found debug message '" << debug << "'.\n";
	}
}

void rectangle_shape::draw(
	const SDL_Rect& portion_to_draw,
	const SDL_Rect& draw_location,
	wfl::map_formula_callable& variables)
{
	const auto rects = calculate_rects(portion_to_draw, variables);
	if(rects.empty) {
		DBG_GUI_D << "Rectangle: nothing to draw" << std::endl;
		return;
	}

	const color_t fill_color = fill_color_(variables);
	const color_t border_color = border_color_(variables);
	SDL_Rect r = rects.unclipped_around_viewport;
	r.x += draw_location.x;
	r.y += draw_location.y;

	DBG_GUI_D << "Rectangle: draw at " << r
		<< " with bounds " << portion_to_draw << std::endl;

	// Fill the background, if applicable
	if(!fill_color.null()) {
		DBG_GUI_D << "fill " << fill_color << std::endl;
		draw::set_color(fill_color);
		SDL_Rect area = r;
		area.x += border_thickness_;
		area.y += border_thickness_;
		area.w -= 2 * border_thickness_;
		area.h -= 2 * border_thickness_;

		draw::fill(area);
	}

	// Draw the border
	draw::set_color(border_color);
	DBG_GUI_D << "border thickness " << border_thickness_
		<< ", colour " << border_color << std::endl;
	for(int i = 0; i < border_thickness_; ++i) {
		SDL_Rect dimensions = r;
		dimensions.x += i;
		dimensions.y += i;
		dimensions.w -= 2 * i;
		dimensions.h -= 2 * i;

		draw::rect(dimensions);
	}
}

/***** ***** ***** ***** ***** Rounded Rectangle ***** ***** ***** ***** *****/

round_rectangle_shape::round_rectangle_shape(const config& cfg)
	: rect_bounded_shape(cfg)
	, r_(cfg["corner_radius"])
	, border_thickness_(cfg["border_thickness"])
	, border_color_(cfg["border_color"], color_t::null_color())
	, fill_color_(cfg["fill_color"], color_t::null_color())
{
	// Check if a raw color string evaluates to a null color.
	if(!border_color_.has_formula() && border_color_().null()) {
		border_thickness_ = 0;
	}

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_GUI_P << "Rounded Rectangle: found debug message '" << debug << "'.\n";
	}
}

void round_rectangle_shape::draw(
	const SDL_Rect& portion_to_draw,
	const SDL_Rect& draw_location,
	wfl::map_formula_callable& variables)
{
	// TODO: highdpi - need to double check this
	const auto rects = calculate_rects(portion_to_draw, variables);
	const int x = draw_location.x + rects.unclipped_around_viewport.x;
	const int y = draw_location.y + rects.unclipped_around_viewport.y;
	const int w = rects.unclipped_around_viewport.w;
	const int h = rects.unclipped_around_viewport.h;
	const int r = r_(variables);

	DBG_GUI_D << "Rounded Rectangle: draw from " << x << ',' << y << " width " << w
		<< " height "
		<< " within bounds {" << portion_to_draw.x << ", " << portion_to_draw.y
		<< ", " << portion_to_draw.w << ", " << portion_to_draw.h << "}.\n";

	const color_t fill_color = fill_color_(variables);

	// Fill the background, if applicable
	if(!fill_color.null() && w && h) {
		draw::set_color(fill_color);

		draw::fill({x + r,                 y + border_thickness_, w - r                 * 2, r - border_thickness_ + 1});
		draw::fill({x + border_thickness_, y + r + 1,             w - border_thickness_ * 2, h - r * 2});
		draw::fill({x + r,                 y - r + h + 1,         w - r                 * 2, r - border_thickness_});

		draw::disc(x + r,     y + r,     r, 0xc0);
		draw::disc(x + w - r, y + r,     r, 0x03);
		draw::disc(x + r,     y + h - r, r, 0x30);
		draw::disc(x + w - r, y + h - r, r, 0x0c);
	}

	const color_t border_color = border_color_(variables);

	// Draw the border
	draw::set_color(border_color);
	for(int i = 0; i < border_thickness_; ++i) {
		draw::line(x + r, y + i,     x + w - r, y + i);
		draw::line(x + r, y + h - i, x + w - r, y + h - i);

		draw::line(x + i,     y + r, x + i,     y + h - r);
		draw::line(x + w - i, y + r, x + w - i, y + h - r);

		draw::circle(x + r,     y + r,     r - i, 0xc0);
		draw::circle(x + w - r, y + r,     r - i, 0x03);
		draw::circle(x + r,     y + h - r, r - i, 0x30);
		draw::circle(x + w - r, y + h - r, r - i, 0x0c);
	}
}

/***** ***** ***** ***** ***** CIRCLE ***** ***** ***** ***** *****/

circle_shape::circle_shape(const config& cfg)
	: shape(cfg)
	, x_(cfg["x"])
	, y_(cfg["y"])
	, radius_(cfg["radius"])
	, border_color_(cfg["border_color"])
	, fill_color_(cfg["fill_color"])
	, border_thickness_(cfg["border_thickness"].to_int(1))
{
	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_GUI_P << "Circle: found debug message '" << debug << "'.\n";
	}
}

void circle_shape::draw(
	const SDL_Rect& portion_to_draw,
	const SDL_Rect& draw_location,
	wfl::map_formula_callable& variables)
{
	/**
	 * @todo formulas are now recalculated every draw cycle which is a bit
	 * silly unless there has been a resize. So to optimize we should use an
	 * extra flag or do the calculation in a separate routine.
	 */

	const int x = draw_location.x + x_(variables) - portion_to_draw.x;
	const int y = draw_location.y + y_(variables) - portion_to_draw.y;
	const unsigned radius = radius_(variables);

	DBG_GUI_D << "Circle: drawn at " << x << ',' << y << " radius " << radius
		<< " within bounds {" << portion_to_draw.x << ", " << portion_to_draw.y
		<< ", " << portion_to_draw.w << ", " << portion_to_draw.h << "}.\n";

	const color_t fill_color = fill_color_(variables);
	if(!fill_color.null() && radius) {
		draw::disc(x, y, radius, fill_color);
	}

	const color_t border_color = border_color_(variables);
	for(unsigned int i = 0; i < border_thickness_; i++) {
		draw::circle(x, y, radius - i, border_color);
	}
}

/***** ***** ***** ***** ***** IMAGE ***** ***** ***** ***** *****/

image_shape::image_shape(const config& cfg, wfl::action_function_symbol_table& functions)
	: shape(cfg)
	, x_(cfg["x"])
	, y_(cfg["y"])
	, w_(cfg["w"])
	, h_(cfg["h"])
	, src_clip_()
	, image_()
	, image_name_(cfg["name"])
	, resize_mode_(get_resize_mode(cfg["resize_mode"]))
	, mirror_(cfg.get_old_attribute("mirror", "vertical_mirror", "image"))
	, actions_formula_(cfg["actions"], &functions)
{
	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_GUI_P << "Image: found debug message '" << debug << "'.\n";
	}
}

void image_shape::dimension_validation(unsigned value, const std::string& name, const std::string& key)
{
	const int as_int = static_cast<int>(value);

	VALIDATE_WITH_DEV_MESSAGE(as_int >= 0, _("Image doesn't fit on canvas."),
		formatter() << "Image '" << name << "', " << key << " = " << as_int << "."
	);
}

void image_shape::draw(
	const SDL_Rect& /*portion_to_draw*/,
	const SDL_Rect& draw_location,
	wfl::map_formula_callable& variables)
{
	DBG_GUI_D << "Image: draw.\n";

	/**
	 * @todo formulas are now recalculated every draw cycle which is a  bit
	 * silly unless there has been a resize. So to optimize we should use an
	 * extra flag or do the calculation in a separate routine.
	 */
	const std::string& name = image_name_(variables);

	if(name.empty()) {
		DBG_GUI_D << "Image: formula returned no value, will not be drawn.\n";
		return;
	}

	// Texture filtering mode must be set on texture creation,
	// so check whether we need smooth scaling or not here.
	image::scale_quality scale_quality = image::scale_quality::nearest;
	if (resize_mode_ == resize_mode::stretch
		|| resize_mode_ == resize_mode::scale)
	{
		scale_quality = image::scale_quality::linear;
	}
	texture tex = image::get_texture(image::locator(name), scale_quality);

	if(!tex) {
		ERR_GUI_D << "Image: '" << name << "' not found and won't be drawn." << std::endl;
		return;
	}

	// TODO: highdpi - this is never used, why is it set?
	src_clip_ = {0, 0, tex.w(), tex.h()};

	wfl::map_formula_callable local_variables(variables);
	local_variables.add("image_original_width", wfl::variant(tex.w()));
	local_variables.add("image_original_height", wfl::variant(tex.h()));

	int w = w_(local_variables);
	dimension_validation(w, name, "w");

	int h = h_(local_variables);
	dimension_validation(h, name, "h");

	local_variables.add("image_width", wfl::variant(w ? w : tex.w()));
	local_variables.add("image_height", wfl::variant(h ? h : tex.h()));

	// TODO: highdpi - why are these called "clip"?
	const unsigned clip_x = x_(local_variables);
	const unsigned clip_y = y_(local_variables);

	// TODO: highdpi - what are these for? They are never used anywhere else.
	local_variables.add("clip_x", wfl::variant(clip_x));
	local_variables.add("clip_y", wfl::variant(clip_y));

	// Execute the provided actions for this context.
	wfl::variant(variables.fake_ptr())
		.execute_variant(actions_formula_.evaluate(local_variables));

	// If w or h is 0, assume it means the whole image.
	if (!w) { w = tex.w(); }
	if (!h) { h = tex.h(); }

	// The image is to be placed at (x,y,w,h) in widget space.
	int x = clip_x;
	int y = clip_y;
	// Convert this to draw space.
	x += draw_location.x;
	y += draw_location.y;
	const SDL_Rect adjusted_draw_loc{x, y, w, h};

	// TODO: highdpi - clipping?

	// What to do with the image depends on whether we need to tile it or not.
	switch (resize_mode_) {
	case (resize_mode::tile):
		draw::tiled(tex, adjusted_draw_loc, false, mirror_(variables));
		break;
	case (resize_mode::tile_center):
		draw::tiled(tex, adjusted_draw_loc, true, mirror_(variables));
		break;
	case resize_mode::tile_highres:
		draw::tiled_highres(tex, adjusted_draw_loc, false, mirror_(variables));
		break;
	case resize_mode::stretch:
		// Stretching is identical to scaling in terms of handling.
		// Is this intended? That's what previous code was doing.
	case resize_mode::scale:
		// Filtering mode is set on texture creation above.
		// Handling is otherwise identical to sharp scaling.
	case resize_mode::scale_sharp:
		if(mirror_(variables)) {
			draw::flipped(tex, adjusted_draw_loc);
		} else {
			draw::blit(tex, adjusted_draw_loc);
		}
		break;
	default:
		ERR_GUI_D << "Image: unrecognized resize mode." << std::endl;
		break;
	}
}

image_shape::resize_mode image_shape::get_resize_mode(const std::string& resize_mode)
{
	if(resize_mode == "tile") {
		return resize_mode::tile;
	} else if(resize_mode == "tile_center") {
		return resize_mode::tile_center;
	} else if(resize_mode == "tile_highres") {
		return resize_mode::tile_highres;
	} else if(resize_mode == "stretch") {
		return resize_mode::stretch;
	} else if(resize_mode == "scale_sharp") {
		return resize_mode::scale_sharp;
	} else {
		if(!resize_mode.empty() && resize_mode != "scale") {
			ERR_GUI_E << "Invalid resize mode '" << resize_mode
					  << "' falling back to 'scale'.\n";
		}
		return resize_mode::scale;
	}
}

/***** ***** ***** ***** ***** TEXT ***** ***** ***** ***** *****/

text_shape::text_shape(const config& cfg)
	: rect_bounded_shape(cfg)
	, font_family_(font::str_to_family_class(cfg["font_family"]))
	, font_size_(cfg["font_size"])
	, font_style_(decode_font_style(cfg["font_style"]))
	, text_alignment_(cfg["text_alignment"])
	, color_(cfg["color"])
	, text_(cfg["text"])
	, text_markup_(cfg["text_markup"], false)
	, link_aware_(cfg["text_link_aware"], false)
	, link_color_(cfg["text_link_color"], color_t::from_hex_string("ffff00"))
	, maximum_width_(cfg["maximum_width"], -1)
	, characters_per_line_(cfg["text_characters_per_line"])
	, maximum_height_(cfg["maximum_height"], -1)
{
	if(!font_size_.has_formula()) {
		VALIDATE(font_size_(), _("Text has a font size of 0."));
	}

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_GUI_P << "Text: found debug message '" << debug << "'.\n";
	}
}

void text_shape::draw(
	const SDL_Rect& area_to_draw,
	const SDL_Rect& draw_location,
	wfl::map_formula_callable& variables)
{
	assert(variables.has_key("text"));

	// We first need to determine the size of the text which need the rendered
	// text. So resolve and render the text first and then start to resolve
	// the other formulas.
	const t_string text = text_(variables);

	if(text.empty()) {
		DBG_GUI_D << "Text: no text to render, leave.\n";
		return;
	}

	font::pango_text& text_renderer = font::get_text_renderer();

	text_renderer
		.set_link_aware(link_aware_(variables))
		.set_link_color(link_color_(variables))
		.set_text(text, text_markup_(variables));

	// TODO: highdpi - determine how the font interface should work. Probably the way it is used here is fine. But the pixel scaling could theoretically be abstracted.
	text_renderer.set_family_class(font_family_)
		.set_font_size(font_size_(variables))
		.set_font_style(font_style_)
		.set_alignment(text_alignment_(variables))
		.set_foreground_color(color_(variables))
		.set_maximum_width(maximum_width_(variables))
		.set_maximum_height(maximum_height_(variables), true)
		.set_ellipse_mode(variables.has_key("text_wrap_mode")
				? static_cast<PangoEllipsizeMode>(variables.query_value("text_wrap_mode").as_int())
				: PANGO_ELLIPSIZE_END)
		.set_characters_per_line(characters_per_line_);

	wfl::map_formula_callable local_variables(variables);
	const auto [tw, th] = text_renderer.get_size();

	// Translate text width and height back to draw-space, rounding up.
	local_variables.add("text_width", wfl::variant(tw));
	local_variables.add("text_height", wfl::variant(th));

	const auto rects = calculate_rects(area_to_draw, local_variables);

	if(rects.empty) {
		DBG_GUI_D << "Text: Clipping to area_to_draw resulted in an empty intersection, nothing to do.\n";
		return;
	}

	// Source region for high-dpi text needs to have pixel scale applied.
	const int pixel_scale = CVideo::get_singleton().get_pixel_scale();
	SDL_Rect clip_in = rects.clip_in_shape;
	clip_in.x *= pixel_scale;
	clip_in.y *= pixel_scale;
	clip_in.w *= pixel_scale;
	clip_in.h *= pixel_scale;

	// Render the currently visible portion of text
	// TODO: highdpi - it would be better to render this all, but some things currently have far too much text. Namely the credits screen.
	texture tex = text_renderer.render_texture(clip_in);
	if(!tex) {
		DBG_GUI_D << "Text: Rendering '" << text << "' resulted in an empty canvas, leave.\n";
		return;
	}

	// Final output - place clipped texture appropriately
	SDL_Rect text_draw_location = draw_location;
	text_draw_location.x += rects.dst_in_viewport.x;
	text_draw_location.x += rects.clip_in_shape.x;
	text_draw_location.y += rects.dst_in_viewport.y;
	text_draw_location.y += rects.clip_in_shape.y;
	text_draw_location.w = rects.dst_in_viewport.w;
	text_draw_location.h = rects.dst_in_viewport.h;
	draw::blit(tex, text_draw_location);
}

/***** ***** ***** ***** ***** CANVAS ***** ***** ***** ***** *****/

canvas::canvas()
	: shapes_()
	, blur_depth_(0)
	, w_(0)
	, h_(0)
	, variables_()
	, functions_()
{
}

canvas::canvas(canvas&& c) noexcept
	: shapes_(std::move(c.shapes_))
	, blur_depth_(c.blur_depth_)
	, w_(c.w_)
	, h_(c.h_)
	, variables_(c.variables_)
	, functions_(c.functions_)
{
}

// TODO: highdpi - draw location specification needs to be completely reworked
void canvas::draw(const SDL_Rect& area_to_draw, const SDL_Rect& draw_location)
{
	log_scope2(log_gui_draw, "Canvas: drawing.");

	// TODO: highdpi - clipping?

	// Draw background.
	if (blur_depth_ && blur_texture_) {
		draw::blit(blur_texture_, draw_location, area_to_draw);
	}

	// draw items
	for(auto& shape : shapes_) {
		lg::scope_logger inner_scope_logging_object__(log_gui_draw, "Canvas: draw shape.");

		// TODO: highdpi - check if child routines benefit from knowing area_to_draw
		shape->draw(area_to_draw, draw_location, variables_);
	}
}

void canvas::blit(SDL_Rect rect)
{
	// This early-return has to come before the `validate(rect.w <= w_)` check, as during the boost_unit_tests execution
	// the debug_clock widget will have no shapes, 0x0 size, yet be given a larger rect to draw.
	if(shapes_.empty()) {
		DBG_GUI_D << "Canvas: empty (no shapes to draw).\n";
		return;
	}

	CVideo& video = CVideo::get_singleton();

	VALIDATE(rect.w >= 0 && rect.h >= 0, _("Area to draw has negative size"));
	VALIDATE(static_cast<unsigned>(rect.w) <= w_ && static_cast<unsigned>(rect.h) <= h_,
		_("Area to draw is larger than widget size"));

	// If the widget is partly off-screen, this might get called with
	// surf width=1000, height=1000
	// rect={-1, 2, 330, 440}
	//
	// From those, as the first column is off-screen:
	// rect_clipped_to_parent={0, 2, 329, 440}
	// area_to_draw={1, 0, 329, 440}
	SDL_Rect parent {0, 0, video.draw_area().w, video.draw_area().h};
	SDL_Rect rect_clipped_to_parent;
	if(!SDL_IntersectRect(&rect, &parent, &rect_clipped_to_parent)) {
		DBG_GUI_D << "Area to draw is completely outside parent.\n";
		return;
	}
	// TODO: highdpi - it is assumed this will never move after blit
	if(blur_depth_ && !blur_texture_) {
		// Cache a blurred image of whatever is underneath.
		surface s = video.read_pixels_low_res(&rect);
		s = blur_surface(s, blur_depth_);
		blur_texture_ = texture(s);
	}

	SDL_Rect area_to_draw {
		std::max(0, -rect.x),
		std::max(0, -rect.y),
		rect_clipped_to_parent.w,
		rect_clipped_to_parent.h
	};

	// `area_to_draw` is the portion of the widget to render,
	// `rect` is the offset to render at.
	draw(area_to_draw, rect);
}

void canvas::parse_cfg(const config& cfg)
{
	log_scope2(log_gui_parse, "Canvas: parsing config.");

	for(const auto shape : cfg.all_children_range())
	{
		const std::string& type = shape.key;
		const config& data = shape.cfg;

		DBG_GUI_P << "Canvas: found shape of the type " << type << ".\n";

		if(type == "line") {
			shapes_.emplace_back(std::make_unique<line_shape>(data));
		} else if(type == "rectangle") {
			shapes_.emplace_back(std::make_unique<rectangle_shape>(data));
		} else if(type == "round_rectangle") {
			shapes_.emplace_back(std::make_unique<round_rectangle_shape>(data));
		} else if(type == "circle") {
			shapes_.emplace_back(std::make_unique<circle_shape>(data));
		} else if(type == "image") {
			shapes_.emplace_back(std::make_unique<image_shape>(data, functions_));
		} else if(type == "text") {
			shapes_.emplace_back(std::make_unique<text_shape>(data));
		} else if(type == "pre_commit") {

			/* note this should get split if more preprocessing is used. */
			for(const auto function : data.all_children_range())
			{

				if(function.key == "blur") {
					blur_depth_ = function.cfg["depth"];
				} else {
					ERR_GUI_P << "Canvas: found a pre commit function"
							  << " of an invalid type " << type << ".\n";
				}
			}

		} else {
			ERR_GUI_P << "Canvas: found a shape of an invalid type " << type
					  << ".\n";

			assert(false);
		}
	}
}

void canvas::update_size_variables()
{
	get_screen_size_variables(variables_);
	variables_.add("width", wfl::variant(w_));
	variables_.add("height", wfl::variant(h_));
}

void canvas::set_size(const point& size)
{
	w_ = size.x;
	h_ = size.y;
	update_size_variables();
}

void canvas::clear_shapes(const bool force)
{
	if(force) {
		shapes_.clear();
	} else {
		auto conditional = [](const std::unique_ptr<shape>& s)->bool { return !s->immutable(); };

		auto iter = std::remove_if(shapes_.begin(), shapes_.end(), conditional);
		shapes_.erase(iter, shapes_.end());
	}
}

/***** ***** ***** ***** ***** SHAPE ***** ***** ***** ***** *****/

} // namespace gui2
