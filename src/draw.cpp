/*
	Copyright (C) 2022
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#include "draw.hpp"

#include "color.hpp"
#include "log.hpp"
#include "sdl/rect.hpp"
#include "sdl/surface.hpp"
#include "sdl/texture.hpp"
#include "video.hpp"

#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>

static lg::log_domain log_draw("draw");
#define DBG_D LOG_STREAM(debug, log_draw)
#define WRN_D LOG_STREAM(warn, log_draw)

using std::endl;

static SDL_Renderer* renderer()
{
	return CVideo::get_singleton().get_renderer();
}

/**************************************/
/* basic drawing and pixel primatives */
/**************************************/

void draw::fill(
	const SDL_Rect& area,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	DBG_D << "fill " << area
	      << " [" << r << ',' << g << ',' << b << ',' << a << ']' << endl;
	SDL_SetRenderDrawColor(renderer(), r, g, b, a);
	SDL_RenderFillRect(renderer(), &area);
}

void draw::fill(
	const SDL_Rect& area,
	uint8_t r, uint8_t g, uint8_t b)
{
	draw::fill(area, r, g, b, SDL_ALPHA_OPAQUE);
}

void draw::fill(const SDL_Rect& area, const color_t& c)
{
	draw::fill(area, c.r, c.g, c.b, c.a);
}

void draw::fill(const SDL_Rect& area)
{
	DBG_D << "fill " << area << endl;
	SDL_RenderFillRect(renderer(), &area);
}

void draw::set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	DBG_D << "set color "
	      << " [" << r << ',' << g << ',' << b << ',' << a << ']' << endl;
	SDL_SetRenderDrawColor(renderer(), r, g, b, a);
}

void draw::set_color(uint8_t r, uint8_t g, uint8_t b)
{
	DBG_D << "set color "
	      << " [" << r << ',' << g << ',' << b << ']' << endl;
	SDL_SetRenderDrawColor(renderer(), r, g, b, SDL_ALPHA_OPAQUE);
}

void draw::set_color(const color_t& c)
{
	DBG_D << "set color " << c << endl;
	SDL_SetRenderDrawColor(renderer(), c.r, c.g, c.b, c.a);
}

void draw::rect(const SDL_Rect& rect)
{
	DBG_D << "rect " << rect << endl;
	SDL_RenderDrawRect(renderer(), &rect);
}

void draw::rect(const SDL_Rect& rect,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	DBG_D << "rect " << rect
	      << " [" << r << ',' << g << ',' << b << ',' << a << ']' << endl;
	SDL_SetRenderDrawColor(renderer(), r, g, b, a);
	SDL_RenderDrawRect(renderer(), &rect);
}

void draw::rect(const SDL_Rect& rect, uint8_t r, uint8_t g, uint8_t b)
{
	draw::rect(rect, r, g, b, SDL_ALPHA_OPAQUE);
}

void draw::rect(const SDL_Rect& rect, const color_t& c)
{
	draw::rect(rect, c.r, c.g, c.b, c.a);
}

void draw::line(int from_x, int from_y, int to_x, int to_y)
{
	DBG_D << "line from (" << from_x << ',' << from_y
	      << ") to (" << to_x << ',' << to_y << ')' << endl;
	SDL_RenderDrawLine(renderer(), from_x, from_y, to_x, to_y);
}

void draw::line(int from_x, int from_y, int to_x, int to_y, const color_t& c)
{
	DBG_D << "line from (" << from_x << ',' << from_y
	      << ") to (" << to_x << ',' << to_y
	      << ") with colour " << c << endl;
	SDL_SetRenderDrawColor(renderer(), c.r, c.g, c.b, c.a);
	SDL_RenderDrawLine(renderer(), from_x, from_y, to_x, to_y);
}

void draw::points(const std::vector<SDL_Point>& points)
{
	DBG_D << points.size() << " points" << endl;
	SDL_RenderDrawPoints(renderer(), points.data(), points.size());
}

void draw::point(int x, int y)
{
	DBG_D << "point (" << x << ',' << y << ')' << endl;
	SDL_RenderDrawPoint(renderer(), x, y);
}

void draw::circle(int cx, int cy, int r, const color_t& c, uint8_t octants)
{
	draw::set_color(c);
	draw::circle(cx, cy, r, octants);
}

void draw::circle(int cx, int cy, int r, uint8_t octants)
{
	DBG_D << "circle (" << cx << ',' << cy
	      << ") -> " << r << ", oct " << int(octants) << endl;

	// Algorithm based on
	// http://de.wikipedia.org/wiki/Rasterung_von_Kreisen#Methode_von_Horn
	// version of 2011.02.07.
	int d = -r;
	int x = r;
	int y = 0;

	std::vector<SDL_Point> points;

	while(!(y > x)) {
		if(octants & 0x04) points.push_back({cx + x, cy + y});
		if(octants & 0x02) points.push_back({cx + x, cy - y});
		if(octants & 0x20) points.push_back({cx - x, cy + y});
		if(octants & 0x40) points.push_back({cx - x, cy - y});

		if(octants & 0x08) points.push_back({cx + y, cy + x});
		if(octants & 0x01) points.push_back({cx + y, cy - x});
		if(octants & 0x10) points.push_back({cx - y, cy + x});
		if(octants & 0x80) points.push_back({cx - y, cy - x});

		d += 2 * y + 1;
		++y;
		if(d > 0) {
			d += -2 * x + 2;
			--x;
		}
	}

	draw::points(points);
}

void draw::disc(int cx, int cy, int r, const color_t& c, uint8_t octants)
{
	draw::set_color(c);
	draw::disc(cx, cy, r, octants);
}

void draw::disc(int cx, int cy, int r, uint8_t octants)
{
	DBG_D << "disc (" << cx << ',' << cy
	      << ") -> " << r << ", oct " << int(octants) << endl;

	int d = -r;
	int x = r;
	int y = 0;

	while(!(y > x)) {
		// I use the formula of Bresenham's line algorithm
		// to determine the boundaries of a segment.
		// The slope of the line is always 1 or -1 in this case.
		if(octants & 0x04)
			// x2 - 1 = y2 - (cy + 1) + cx
			draw::line(cx + x, cy + y + 1, cx + y + 1, cy + y + 1);
		if(octants & 0x02)
			// x2 - 1 = cy - y2 + cx
			draw::line(cx + x, cy - y, cx + y + 1, cy - y);
		if(octants & 0x20)
			// x2 + 1 = (cy + 1) - y2 + (cx - 1)
			draw::line(cx - x - 1, cy + y + 1, cx - y - 2, cy + y + 1);
		if(octants & 0x40)
			// x2 + 1 = y2 - cy + (cx - 1)
			draw::line(cx - x - 1, cy - y, cx - y - 2, cy - y);

		if(octants & 0x08)
			// y2 = x2 - cx + (cy + 1)
			draw::line(cx + y, cy + x + 1, cx + y, cy + y + 1);
		if(octants & 0x01)
			// y2 = cx - x2 + cy
			draw::line(cx + y, cy - x, cx + y, cy - y);
		if(octants & 0x10)
			// y2 = (cx - 1) - x2 + (cy + 1)
			draw::line(cx - y - 1, cy + x + 1, cx - y - 1, cy + y + 1);
		if(octants & 0x80)
			// y2 = x2 - (cx - 1) + cy
			draw::line(cx - y - 1, cy - x, cx - y - 1, cy - y);

		d += 2 * y + 1;
		++y;
		if(d > 0) {
			d += -2 * x + 2;
			--x;
		}
	}
}


/*******************/
/* texture drawing */
/*******************/


void draw::blit(const texture& tex, const SDL_Rect& dst, const SDL_Rect& src)
{
	if (!tex) { DBG_D << "null blit" << endl; return; }
	DBG_D << "blit " << dst << " from " << src << endl;

	SDL_RenderCopy(renderer(), tex, &src, &dst);
}

void draw::blit(const texture& tex, const SDL_Rect& dst)
{
	if (!tex) { DBG_D << "null blit" << endl; return; }
	DBG_D << "blit " << dst << endl;

	SDL_RenderCopy(renderer(), tex, nullptr, &dst);
}

void draw::blit(const texture& tex)
{
	if (!tex) { DBG_D << "null blit" << endl; return; }
	DBG_D << "blit" << endl;

	SDL_RenderCopy(renderer(), tex, nullptr, nullptr);
}


static SDL_RendererFlip get_flip(bool flip_h, bool flip_v)
{
	// This should be easier than it is.
	return static_cast<SDL_RendererFlip>(
		static_cast<int>(flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE)
		| static_cast<int>(flip_v ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE)
	);
}

void draw::flipped(
	const texture& tex,
	const SDL_Rect& dst,
	const SDL_Rect& src,
	bool flip_h,
	bool flip_v)
{
	if (!tex) { DBG_D << "null flipped" << endl; return; }
	DBG_D << "flipped (" << flip_h << '|' << flip_v
	      << ") to " << dst << " from " << src << endl;

	SDL_RendererFlip flip = get_flip(flip_h, flip_v);
	SDL_RenderCopyEx(renderer(), tex, &src, &dst, 0.0, nullptr, flip);
}

void draw::flipped(
	const texture& tex,
	const SDL_Rect& dst,
	bool flip_h,
	bool flip_v)
{
	if (!tex) { DBG_D << "null flipped" << endl; return; }
	DBG_D << "flipped (" << flip_h << '|' << flip_v
	      << ") to " << dst << endl;

	SDL_RendererFlip flip = get_flip(flip_h, flip_v);
	SDL_RenderCopyEx(renderer(), tex, nullptr, &dst, 0.0, nullptr, flip);
}

void draw::flipped(const texture& tex, bool flip_h, bool flip_v)
{
	if (!tex) { DBG_D << "null flipped" << endl; return; }
	DBG_D << "flipped (" << flip_h << '|' << flip_v << ')' << endl;

	SDL_RendererFlip flip = get_flip(flip_h, flip_v);
	SDL_RenderCopyEx(renderer(), tex, nullptr, nullptr, 0.0, nullptr, flip);
}


// TODO: highdpi - maybe expose this mirrored mode to WML somehow
void draw::tiled(const texture& tex, const SDL_Rect& dst, bool centered,
	bool mirrored)
{
	if (!tex) { DBG_D << "null tiled" << endl; return; }
	DBG_D << "tiled (" << centered << '|' << mirrored
	      << ") " << dst << endl;

	// Reduce clip to dst.
	auto clipper = draw::reduce_clip(dst);

	const int xoff = centered ? (dst.w - tex.w()) / 2 : 0;
	const int yoff = centered ? (dst.h - tex.h()) / 2 : 0;

	// Just blit the image however many times is necessary.
	bool vf = false;
	SDL_Rect t{dst.x - xoff, dst.y - yoff, tex.w(), tex.h()};
	for (; t.y < dst.y + dst.h; t.y += t.h, vf = !vf) {
		bool hf = false;
		for (t.x = dst.x - xoff; t.x < dst.x + dst.w; t.x += t.w, hf = !hf) {
			if (mirrored) {
				draw::flipped(tex, t, hf, vf);
			} else {
				draw::blit(tex, t);
			}
		}
	}
}

void draw::tiled_highres(const texture& tex, const SDL_Rect& dst,
	bool centered, bool mirrored)
{
	if (!tex) { DBG_D << "null tiled_highres" << endl; return; }
	DBG_D << "tiled_highres (" << centered << '|' << mirrored
	      << ") " << dst << endl;

	const int pixel_scale = CVideo::get_singleton().get_pixel_scale();

	// Reduce clip to dst.
	auto clipper = draw::reduce_clip(dst);

	const auto info = tex.get_info();
	const float w = float(info.w) / float(pixel_scale);
	const float h = float(info.h) / float(pixel_scale);
	const float xoff = centered ? (dst.w - w) / 2 : 0.0f;
	const float yoff = centered ? (dst.h - h) / 2 : 0.0f;

	// Just blit the image however many times is necessary.
	bool vf = false;
	SDL_FRect t{dst.x - xoff, dst.y - yoff, w, h};
	for (; t.y < dst.y + dst.h; t.y += t.h, vf = !vf) {
		bool hf = false;
		for (t.x = dst.x - xoff; t.x < dst.x + dst.w; t.x += t.w, hf = !hf) {
			if (mirrored) {
				SDL_RendererFlip flip = get_flip(hf, vf);
				SDL_RenderCopyExF(renderer(), tex, nullptr, &t, 0.0, nullptr, flip);
			} else {
				SDL_RenderCopyF(renderer(), tex, nullptr, &t);
			}
		}
	}
}


/***************************/
/* RAII state manipulation */
/***************************/


draw::clip_setter::clip_setter(const SDL_Rect& clip)
	: c_()
{
	c_ = draw::get_clip();
	draw::force_clip(clip);
}

draw::clip_setter::~clip_setter()
{
	draw::force_clip(c_);
}

draw::clip_setter draw::set_clip(const SDL_Rect& clip)
{
	return draw::clip_setter(clip);
}

draw::clip_setter draw::reduce_clip(const SDL_Rect& clip)
{
	SDL_Rect c = draw::get_clip();
	if (c == sdl::empty_rect) {
		return draw::clip_setter(clip);
	} else {
		return draw::clip_setter(sdl::intersect_rects(clip, c));
	}
}

void draw::force_clip(const SDL_Rect& clip)
{
	// TODO: highdpi - fix whatever reason there is for this guard (CI fail)
	if (!renderer()) {
		WRN_D << "trying to force clip will null renderer" << endl;
		return;
	}
	DBG_D << "forcing clip to " << clip << endl;

	SDL_RenderSetClipRect(renderer(), &clip);
}

SDL_Rect draw::get_clip()
{
	// TODO: highdpi - fix whatever reason there is for this guard (CI fail)
	if (!renderer()) {
		return sdl::empty_rect;
	}

	SDL_Rect clip;
	SDL_RenderGetClipRect(renderer(), &clip);

	if (clip == sdl::empty_rect) {
		// TODO: highdpi - fix this in the case of render to texture
		return CVideo::get_singleton().draw_area();
	}
	return clip;
}


draw::viewport_setter::viewport_setter(const SDL_Rect& view)
	: v_()
{
	v_ = draw::get_viewport();
	draw::force_viewport(view);
}

draw::viewport_setter::~viewport_setter()
{
	draw::force_viewport(v_);
}

draw::viewport_setter draw::set_viewport(const SDL_Rect& viewport)
{
	return draw::viewport_setter(viewport);
}

void draw::force_viewport(const SDL_Rect& viewport)
{
	if (!renderer()) {
		WRN_D << "trying to force viewport will null renderer" << endl;
		return;
	}
	DBG_D << "forcing viewport to " << viewport << endl;

	SDL_RenderSetViewport(renderer(), &viewport);
}

SDL_Rect draw::get_viewport()
{
	if (!renderer()) {
		WRN_D << "no renderer available to get viewport" << endl;
		return sdl::empty_rect;
	}

	SDL_Rect viewport;
	SDL_RenderGetViewport(renderer(), &viewport);

	if (viewport == sdl::empty_rect) {
		// TODO: highdpi - fix this in the case of render to texture
		return CVideo::get_singleton().draw_area();
	}
	return viewport;
}


draw::render_target_setter::render_target_setter(const texture& t)
	: target_(nullptr)
	, viewport_()
{
	// Validate we can render to this texture.
	assert(t.get_info().access == SDL_TEXTUREACCESS_TARGET);

	if (!renderer()) {
		WRN_D << "can't set render target with null renderer" << endl;
		return;
	}

	target_ = CVideo::get_singleton().get_render_target();
	SDL_RenderGetViewport(renderer(), &viewport_);

	CVideo::get_singleton().force_render_target(t);
}

draw::render_target_setter::~render_target_setter()
{
	if (!renderer()) {
		WRN_D << "can't reset render target with null renderer" << endl;
		return;
	}
	CVideo::get_singleton().force_render_target(target_);
	SDL_RenderSetViewport(renderer(), &viewport_);
}

draw::render_target_setter draw::set_render_target(const texture& t)
{
	DBG_D << "setting render target to "
	      << t.w() << 'x' << t.h() << " texture" << endl;
	return draw::render_target_setter(t);
}
