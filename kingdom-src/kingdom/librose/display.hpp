/* $Id: display.hpp 47673 2010-11-21 14:32:46Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

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
 *
 * map_display and display: classes which take care of
 * displaying the map and game-data on the screen.
 *
 * The display is divided into two main sections:
 * - the game area, which displays the tiles of the game board, and units on them,
 * - and the side bar, which appears on the right hand side.
 * The side bar display is divided into three sections:
 * - the minimap, which is displayed at the top right
 * - the game status, which includes the day/night image,
 *   the turn number, information about the current side,
 *   and information about the hex currently moused over (highlighted)
 * - the unit status, which displays an image and stats
 *   for the current unit.
 */

#ifndef DISPLAY_H_INCLUDED
#define DISPLAY_H_INCLUDED

class config;
class terrain_builder;
struct time_of_day;
class map_labels;
class arrow;
class base_unit;

#include "font.hpp"
#include "key.hpp"
#include "reports.hpp"
#include "video.hpp"
#include "animation.hpp"
#include "area_anim.hpp"
#include "gui/widgets/control.hpp"
#include "gui/dialogs/theme.hpp"
#include "generic_event.hpp"

#include <list>

#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

class gamemap;
class controller_base;

/**
 * Rectangular area of hexes, allowing to decide how the top and bottom
 * edges handles the vertical shift for each parity of the x coordinate
 */
struct rect_of_hexes 
{
	int left;
	int right;
	int top[2]; // for even and odd values of x, respectively
	int bottom[2];

	/**  very simple iterator to walk into the rect_of_hexes */
	struct iterator {
		iterator(const map_location &loc, const rect_of_hexes &rect)
			: loc_(loc), rect_(rect){};

		/** increment y first, then when reaching bottom, increment x */
		void operator++();
		bool operator==(const iterator &that) const { return that.loc_ == loc_; }
		bool operator!=(const iterator &that) const { return that.loc_ != loc_; }
		const map_location& operator*() const {return loc_;};

		typedef std::forward_iterator_tag iterator_category;
		typedef map_location value_type;
		typedef int difference_type;
		typedef const map_location *pointer;
		typedef const map_location &reference;

		private:
			map_location loc_;
			const rect_of_hexes &rect_;
	};
	typedef iterator const_iterator;

	iterator begin() const;
	iterator end() const;
};

struct tsurf_buf
{
	tsurf_buf()
		: rect(create_rect(0, 0, 0, 0))
	{}

	SDL_Rect rect;  // based on screen coordinate
	surface surf;  // foreground surface
	surface buffer; // background surface
};

#define ZOOM_INCREMENT		4

#define point_in_rect_of_hexes(x, y, rect)	\
	((x) >= (rect).left && (y) >= (rect).top[(x) & 1] && (x) <= (rect).right && (y) <= (rect).bottom[(x) & 1])

#define draw_area_index(x, y)	((y) + map_border_size_) * draw_area_pitch_ + ((x) + map_border_size_)
#define draw_area_val(x, y)		draw_area_[((y) + map_border_size_) * draw_area_pitch_ + ((x) + map_border_size_)]

class display
{
public:
	class tcanvas_drawing_buffer_lock
	{
	public:
		tcanvas_drawing_buffer_lock(display& disp);
		~tcanvas_drawing_buffer_lock();

	private:
		display& disp_;
		bool to_canvas_;
	};

	static bool require_change_resolution;
	static int initial_zoom;

	enum {ZOOM_72 = 72, ZOOM_64 = 64, ZOOM_56 = 56, ZOOM_48 = 48};
	static int adjust_zoom(int zoom);

	display(const std::string& tile, controller_base* controller, CVideo& video, const gamemap* map, const config& theme_cfg,
			const config& level, size_t num_reports);
	virtual ~display();
	static display* get_singleton() { return singleton_ ;}
	static void set_singleton(display* s) { singleton_ = s; }

	static display* create_dummy_display(CVideo& video);

	/**
	 * Updates internals that cache map size. This should be called when the map
	 * size has changed.
	 */
	void reload_map();

	void change_map(const gamemap* m);

	static Uint32 rgb(Uint8 red, Uint8 green, Uint8 blue)
		{ return 0xFF000000 | (red << 16) | (green << 8) | blue; }
	static Uint8 red(Uint32 color)
		{ return (color & 0x00FF0000) >> 16;}
	static Uint8 green(Uint32 color)
		{ return (color & 0x0000FF00) >> 8;}
	static Uint8 blue(Uint32 color)
		{ return (color & 0x000000FF) ;}
	static Uint32 max_rgb(Uint32 first,Uint32 second)
		{ return rgb(std::max(red(first),red(second)),std::max(green(first),green(second)),std::max(blue(first),blue(second))) ; }

	/** Gets the underlying screen object. */
	CVideo& video() { return screen_; }

	/** return the screen surface or the surface used for map_screenshot. */
	surface get_screen_surface() { return map_screenshot_ ? map_screenshot_surf_ : screen_.getSurface();}

	// virtual bool in_game() const { return false; }
	virtual bool in_theme() const { return false; }

	/**
	 * the dimensions of the display. x and y are width/height.
	 * mapx is the width of the portion of the display which shows the game area.
	 * Between mapx and x is the sidebar region.
	 */
	int w() const { return screen_.getx(); }	/**< width */
	int h() const { return screen_.gety(); }	/**< height */
	const SDL_Rect& minimap_area() const;

	SDL_Rect screen_area() const
		{ return create_rect(0, 0, w(), h()); }

	/**
	 * Returns the maximum area used for the map
	 * regardless to resolution and view size
	 */
	virtual const SDL_Rect& max_map_area() const;

	/**
	 * Returns the area used for the map
	 */
	const SDL_Rect& map_area() const;

	/**
	 * Returns the available area for a map, this may differ
	 * from the above. This area will get the background area
	 * applied to it.
	 */
	const SDL_Rect& map_outside_area() const { return map_screenshot_ ?
		max_map_area() : main_map_area_; }

	/** Check if the bbox of the hex at x,y has pixels outside the area rectangle. */
	bool outside_area(const SDL_Rect& area, const int x,const int y) const;

	int zoom() const { return zoom_; }

	/**
	 * Function which returns the width of a hex in pixels,
	 * up to where the next hex starts.
	 * (i.e. not entirely from tip to tip -- use hex_size()
	 * to get the distance from tip to tip)
	 */
	virtual int hex_width() const { return zoom_; }

	/**
	 * Function which returns the size of a hex in pixels
	 * (from top tip to bottom tip or left edge to right edge).
	 */
	virtual int hex_size() const { return zoom_; }

	/** Returns the current zoom factor. */
	double get_zoom_factor() const;

	/**
	 * given x,y co-ordinates of an onscreen pixel, will return the
	 * location of the hex that this pixel corresponds to.
	 * Returns an invalid location if the mouse isn't over any valid location.
	 */
	const map_location hex_clicked_on(int x, int y) const;

	void pixel_screen_to_map(int& xclick, int& yclick) const;

	/**
	 * given x,y co-ordinates of a pixel on the map, will return the
	 * location of the hex that this pixel corresponds to.
	 * Returns an invalid location if the mouse isn't over any valid location.
	 */
	virtual const map_location pixel_position_to_hex(int x, int y) const;

	/**
	 * given x,y co-ordinates of the mouse, will return the location of the
	 * hex in the minimap that the mouse is currently over, or an invalid
	 * location if the mouse isn't over the minimap.
	 */
	map_location minimap_location_on(int x, int y);

	const map_location& selected_hex() const { return selectedHex_; }
	const map_location& mouseover_hex() const { return mouseoverHex_; }

	virtual void select_hex(map_location hex);
	virtual void highlight_hex(map_location hex);

	/** Function to invalidate the game status displayed on the sidebar. */
	void invalidate_game_status() { invalidateGameStatus_ = true; }

	/** Functions to get the on-screen positions of hexes. */
	virtual int get_location_x(const map_location& loc) const;
	virtual int get_location_y(const map_location& loc) const;

	int get_scroll_pixel_x(int x) const;
	int get_scroll_pixel_y(int y) const;

	/** Return the rectangular area of hexes overlapped by r (r is in screen coordinates) */
	virtual const rect_of_hexes hexes_under_rect(const SDL_Rect& r) const;

	/** Returns the rectangular area of visible hexes */
	const rect_of_hexes get_visible_hexes() const { return hexes_under_rect(map_area());};

	/** Returns true if location (x,y) is covered in shroud. */
	virtual bool shrouded(const map_location& loc) const { return false; }

	/** Returns true if location (x,y) is covered in fog. */
	virtual bool fogged(const map_location& loc) const { return false; }

	/**
	 * Determines whether a grid should be overlayed on the game board.
	 * (to more clearly show where hexes are)
	 */
	void set_grid(const bool grid) { grid_ = grid; }

	/** Getter for the x,y debug overlay on tiles */
	bool get_draw_coordinates() const { return draw_coordinates_; }
	/** Setter for the x,y debug overlay on tiles */
	void set_draw_coordinates(bool value) { draw_coordinates_ = value; }

	/** Getter for the terrain code debug overlay on tiles */
	bool get_draw_terrain_codes() const { return draw_terrain_codes_; }
	/** Setter for the terrain code debug overlay on tiles */
	void set_draw_terrain_codes(bool value) { draw_terrain_codes_ = value; }

	/** Save a (map-)screenshot and return the estimated file size */
	int screenshot(std::string filename, bool map_screenshot = false);

	/** Invalidates entire screen, including all tiles and sidebar. Calls redraw observers. */
	void redraw_everything();
	void change_resolution();
	virtual void pre_change_resolution(std::map<const std::string, bool>& actives) {}
	virtual void post_change_resolution(const std::map<const std::string, bool>& actives) {}

	void click_context_menu(const std::string& main, const std::string& id, size_t flags);
	void show_context_menu(const std::string& main = null_str, const std::string& id = null_str);
	void hide_context_menu(const std::string& main = null_str);

	/** Adds a redraw observer, a function object to be called when redraw_everything is used */
	void add_redraw_observer(boost::function<void(display&)> f);

	/** Clear the redraw observers */
	void clear_redraw_observers();

	void widget_set_pip_image(const std::string& id, const std::string& bg, const std::string& fg);
	void widget_set_image(const std::string& id, const std::string& image);
	void widget_set_surface(const std::string& id, const surface& surf);

	void refresh_report(int num, const reports::report& r);
	virtual surface refresh_surface_report(int num, const reports::report& r, gui2::twidget& widget) { return surface(); }

	// runtime tooltip of mouse-over unit
	virtual void hide_tip() {}

	// Will be overridden in the display subclass
	virtual void draw_minimap_units(surface& screen) {};

	/** Function to invalidate all tiles. */
	void invalidate_all();

	/** Function to invalidate a specific tile for redrawing. */
	bool invalidate(const map_location& loc);

	bool invalidate(const std::set<map_location>& locs);

	/**
	 * If this set is partially invalidated, invalidate all its hexes.
	 * Returns if any new invalidation was needed
	 */
	bool propagate_invalidation(const std::set<map_location>& locs);

	/** invalidate all hexes under the rectangle rect (in screen coordinates) */
	bool invalidate_locations_in_rect(const SDL_Rect& rect);
	bool invalidate_visible_locations_in_rect(const SDL_Rect& rect);

	/**
	 * Function to invalidate animated terrains which may have changed.
	 */
	virtual void invalidate_animations();

	virtual void invalidate_theme();

	/**
	 * Per-location invalidation called by invalidate_animations()
	 * defaults to no action, overridden by derived classes
	 */
	virtual void invalidate_animations_location(const map_location& /*loc*/) {}


	const gamemap& get_map() const { return *map_; }

	/**
	 * mouseover_hex_overlay_ require a prerendered surface
	 * and is drawn underneath the mouse's location
	 */
	void set_mouseover_hex_overlay(const surface& image)
		{ mouseover_hex_overlay_ = image; }

	void clear_mouseover_hex_overlay()
		{ mouseover_hex_overlay_ = NULL; }

	/**
	 * Debug function to toggle the "sunset" mode.
	 * The map area become progressively darker,
	 * except where hexes are refreshed.
	 * delay is the number of frames between each darkening
	 * (0 to toggle).
	 */
	static void sunset(const size_t delay = 0);

	terrain_builder& get_builder() {return *builder_;};

	void flip();

	/** Rebuild all dynamic terrain. */
	virtual void rebuild_all();

	/**
	 * Finds the menu which has a given item in it,
	 * and hides or shows it.
	 */
	virtual void goto_main_context_menu();

	/**
	 * Add a location to highlight.
	 *
	 * Note that this has nothing to do with selecting hexes,
	 * it is pure highlighting. These hexes will be highlighted
	 * slightly darker than the currently selected hex.
	 */
	void add_highlighted_loc(const map_location &hex);

	void clear_highlighted_locs();

	void remove_highlighted_loc(const map_location &hex);

	enum LOC_FLAGS {BOARD, INVALIDATE, INVALIDATE_UNIT};
	rect_of_hexes& draw_area();
	const rect_of_hexes& draw_area() const { return draw_area_rect_; }

	/**
	 * Finds the menu which has a given item in it,
	 * and enables or disables it.
	 */
	void set_diagnostic(const std::string& msg);

	/** Delay routines: use these not SDL_Delay (for --nogui). */
	void delay(unsigned int milliseconds) const;

	/**
	 * Set/Get whether 'turbo' mode is on.
	 * When turbo mode is on, everything moves much faster.
	 */
	void set_turbo(const bool turbo) { turbo_ = turbo; }

	double turbo_speed() const;

	void set_turbo_speed(const double speed) { turbo_speed_ = speed; }

	void bounds_check_position();
	void bounds_check_position(int& xpos, int& ypos);

	/**
	 * Scrolls the display by xmov,ymov pixels.
	 * Invalidation and redrawing will be scheduled.
	 * @return true if the map actually moved.
	 */
	bool scroll(int xmov, int ymov);

	/**
	 * Zooms the display by the specified amount.
	 * Negative values zoom out.
	 * Note the amount should be a multiple of four,
	 * otherwise the images might start to look odd
	 * (hex_width() gets rounding errors).
	 */
	void set_zoom(int amount);

	/** Sets the zoom amount to the default. */
	void set_default_zoom();

	enum SCROLL_TYPE { SCROLL, WARP, ONSCREEN };

	/**
	 * Scroll such that location loc is on-screen.
	 * WARP jumps to loc; SCROLL uses scroll speed;
	 * ONSCREEN only scrolls if x,y is offscreen
	 * force : scroll even if prefferences tell us not to
	 */
	void scroll_to_tile(const map_location& loc, SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,bool force = true);

	/**
	 * Scroll such that location loc1 is on-screen.
	 * It will also try to make it such that loc2 is on-screen,
	 * but this is not guaranteed. For ONSCREEN scrolls add_spacing
	 * sets the desired minimum distance from the border in hexes.
	 */
	void scroll_to_tiles(map_location loc1, map_location loc2,
	                     SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,
			     double add_spacing=0.0, bool force = true);

	/** Scroll to fit as many locations on-screen as possible, starting with the first. */
	void scroll_to_tiles(const std::vector<map_location>& locs,
	                     SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,
	                     bool only_if_possible=false,
			     double add_spacing=0.0, bool force = true);

	void scroll_to_xy(int screenxpos, int screenypos, SCROLL_TYPE scroll_type, bool force = true);

	/** Expose the event, so observers can be notified about map scrolling. */
	events::generic_event &scroll_event() const { return scroll_event_; }

	/** Check if a tile is fully visible on screen. */
	bool tile_fully_on_screen(const map_location& loc);

	/** Checks if location @a loc or one of the adjacent tiles is visible on screen. */
	bool tile_nearly_on_screen(const map_location &loc) const;

	/**
	 * Draws invalidated items.
	 * If update is true, will also copy the display to the frame buffer.
	 * If force is true, will not skip frames, even if running behind.
	 * Not virtual, since it gathers common actions. Calls various protected
	 * virtuals (further below) to allow specialized behaviour in derived classes.
	 */
	void draw(bool update=true, bool force=false);

	map_labels& labels();
	const map_labels& labels() const;

	/** Announce a message prominently. */
	void announce(const std::string& msg,
		       const SDL_Color& color = font::GOOD_COLOR);

	/**
	 * Schedule the minimap for recalculation.
	 * Useful if any terrain in the map has changed.
	 */
	void recalculate_minimap() {minimap_ = NULL; redrawMinimap_ = true; };

	/**
	 * Schedule the minimap to be redrawn.
	 * Useful if units have moved about on the map.
	 */
	void redraw_minimap() { redrawMinimap_ = true; }

	virtual const time_of_day& get_time_of_day(const map_location& /*loc*/) const;

	virtual bool has_time_area() const {return false;};

	/** Function to float a label above a tile */
	void float_label(const map_location& loc, const std::string& text,
	                 int red, int green, int blue, bool slow = false);

	std::map<int, animation*>& area_anims() { return area_anims_; }
	animation& area_anim(int id);

	virtual int insert_area_anim(const animation& tpl);
	int insert_area_anim2(animation* anim);
	virtual void erase_area_anim(int id);
	void clear_area_anims();
	void draw_float_anim();
	void undraw_float_anim();

	gui2::ttheme* get_theme() { return theme_; }
	gui2::twidget* get_theme_object(const std::string& id) const ;
	void set_theme_object_active(const std::string& id, bool active) const;
	void set_theme_object_visible(const std::string& id, const gui2::twidget::tvisible visible) const;
	void set_theme_object_surface(const std::string& id, const surface& surf) const;
	gui2::twidget* get_theme_report(int num) const;
	void set_theme_report_label(int num, const std::string& label) const;
	void set_theme_report_surface(int num, const surface& surf) const;

	virtual std::string get_theme_patch() const { return null_str; }
	virtual gui2::ttheme* create_theme_dlg(const config& cfg) { return NULL; }
	const reports::report& cached_report(int num) const { return reports_[num]; }

	int min_zoom() const { return min_zoom_; }
	int max_zoom() const { return max_zoom_; }
	bool point_in_volatiles(int x, int y) const;

	virtual void shrouded_and_fogged(const map_location& loc, bool& shrouded, bool& fogged) const {}

	// valid on iOS
	void set_statusbar(bool show, bool white_fg);

protected:
	/** Clear the screen contents */
	void clear_screen();

	/**
	 * Called near the beginning of each draw() call.
	 * Derived classes can use this to add extra actions before redrawing
	 * invalidated hexes takes place. No action here by default.
	 */
	virtual void pre_draw(rect_of_hexes& hexes);

	/**
	 * Get the clipping rectangle for drawing.
	 * Virtual since the editor might use a slightly different approach.
	 */
	virtual const SDL_Rect& get_clip_rect();

	/**
	 * Only called when there's actual redrawing to do. Loops through
	 * invalidated locations and redraws them. Derived classes can override
	 * this, possibly to insert pre- or post-processing around a call to the
	 * base class's function.
	 */
	virtual void draw_invalidated();

	/**
	 * Hook for actions to take right after draw() calls drawing_buffer_commit
	 * No action here by default.
	 */
	void post_commit();

	/**
	 * Redraws a single gamemap location.
	 */
	virtual void draw_hex(const map_location& loc);

	/**
	 * @returns the image type to be used for the passed hex
	 * (mostly to do with brightening like for mouseover)
	 */
	virtual image::TYPE get_image_type(const map_location& loc);

	/**
	 * Called near the end of a draw operation, derived classes can use this
	 * to render a specific sidebar. Very similar to post_commit.
	 */
	virtual void draw_sidebar();

	/**
	 * Draws the border tile overlay.
	 * The routine determines by itself which border it is on
	 * and draws an overlay accordingly. The definition of the
	 * border is stored in the 'main_map_border' part of the theme.
	 *
	 * @param loc	the map location of the tile
	 * @param xpos	the on-screen pixels x coordinate of the tile
	 * @param ypos	the on-screen pixels y coordinate of the tile
	 */
	virtual void draw_border(const map_location& loc, const int xpos, const int ypos);

	virtual surface minimap_surface(int w, int h);
	void draw_minimap();

	enum TERRAIN_TYPE { BACKGROUND, FOREGROUND};

	std::vector<surface> get_terrain_images(const map_location &loc,
					const std::string& timeid,
					image::TYPE type,
					TERRAIN_TYPE terrain_type);

	std::vector<surface> get_fog_shroud_images(const map_location& loc, image::TYPE image_type);

	void draw_image_for_report(surface& img, SDL_Rect& rect);

	void fill_images_list(const std::string& prefix, std::vector<std::string>& images);

	const std::string& get_variant(const std::vector<std::string>& variants, const map_location &loc) const;

	virtual double minimap_shift_x(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const;
	virtual double minimap_shift_y(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const;

	virtual void post_set_zoom(int last_zoom) {}

protected:
	CVideo& screen_;
	const gamemap* map_;
	int xpos_, ypos_;
	int zoom_;
	boost::scoped_ptr<terrain_builder> builder_;
	surface minimap_;
	SDL_Rect minimap_location_;
	bool redrawMinimap_;
	bool redraw_background_;
	bool invalidateAll_;
	bool grid_;
	int diagnostic_label_;
	double turbo_speed_;
	bool turbo_;
	bool invalidateGameStatus_;
	boost::scoped_ptr<map_labels> map_labels_;

	int last_map_w_;
	int last_map_h_;

	gui2::tpoint zero_;
	bool portrait_;
	bool always_bottom_;

	/** Event raised when the map is being scrolled */
	mutable events::generic_event scroll_event_;

	/**
	 * Holds the tick count for when the next drawing event is scheduled.
	 * Drawing shouldn't occur before this time.
	 */
	int nextDraw_;

	// Not set by the initializer:
	std::vector<reports::report> reports_;
	surface mouseover_hex_overlay_;
	// If we're transitioning from one time of day to the next,
	// then we will use these two masks on top of all hexes when we blit.
	surface tod_hex_mask1, tod_hex_mask2;
	std::vector<std::string> fog_images_;
	std::vector<std::string> shroud_images_;

	map_location selectedHex_;
	map_location mouseoverHex_;
	bool show_hover_over_;

	CKey keys_;

	/** Local cache for preferences::animate_map, since it is constantly queried. */
	bool animate_map_;

	/** Local cache for preferences "local_tod_light" */
	bool local_tod_light_;

	uint8_t* draw_area_;
	int draw_area_pitch_;
	int draw_area_size_;
	bool drawing_;
	rect_of_hexes draw_area_rect_;
	int map_border_size_;
	// for draw
	base_unit** draw_area_unit_;
	size_t draw_area_unit_size_;

	// used for runtime tooltip of mouse-over unit
	int main_tip_handle_;
public:
	/**
	 * The layers to render something on. This value should never be stored
	 * it's the internal drawing order and adding removing and reordering
	 * the layers should be safe.
	 * If needed in WML use the name and map that to the enum value.
	 */
	enum tdrawing_layer{
		LAYER_TERRAIN_BG,          /**<
		                            * Layer for the terrain drawn behind the
		                            * unit.
		                            */
		LAYER_GRID_TOP,            /**< Top half part of grid image */
		LAYER_MOUSEOVER_OVERLAY,   /**< Mouseover overlay used by editor*/
		LAYER_FOOTSTEPS,           /**< Footsteps showing path from unit to mouse */
		LAYER_MOUSEOVER_TOP,       /**< Top half of image following the mouse */
		LAYER_UNIT_FIRST,          /**< Reserve layers to be selected for WML. */
		LAYER_UNIT_BG = LAYER_UNIT_FIRST+10,             /**< Used for the ellipse behind the unit. */
		LAYER_UNIT_DEFAULT=LAYER_UNIT_FIRST+40,/**<default layer for drawing units */
		LAYER_TERRAIN_FG = LAYER_UNIT_FIRST+50, /**<
		                            * Layer for the terrain drawn in front of
		                            * the unit.
		                            */
		LAYER_GRID_BOTTOM,         /**<
		                            * Used for the bottom half part of grid image.
		                            * Should be under moving units, to avoid masking south move.
		                            */
		LAYER_UNIT_MOVE_DEFAULT=LAYER_UNIT_FIRST+60/**<default layer for drawing moving units */,
		LAYER_UNIT_FG =  LAYER_UNIT_FIRST+80, /**<
		                            * Used for the ellipse in front of the
		                            * unit.
		                            */
		LAYER_UNIT_MISSILE_DEFAULT = LAYER_UNIT_FIRST+90, /**< default layer for missile frames*/
		LAYER_UNIT_LAST=LAYER_UNIT_FIRST+100,
		LAYER_UNIT_BAR=LAYER_UNIT_LAST+10,            /**<
		                            * Unit bars and overlays are drawn on this
		                            * layer (for testing here).
		                            */
		LAYER_REACHMAP,            /**< "black stripes" on unreachable hexes. */
		LAYER_MOUSEOVER_BOTTOM,    /**< Bottom half of image following the mouse */
		LAYER_FOG_SHROUD,          /**< Fog and shroud. */
		LAYER_ARROWS,              /**< Arrows from the arrows framework. Used for planned moves display. */
		LAYER_SELECTED_HEX,        /**< Image on the selected unit */
		LAYER_ATTACK_INDICATOR,    /**< Layer which holds the attack indicator. */

		LAYER_MOVE_INFO,           /**< Movement info (defense%, ect...). */
		LAYER_LINGER_OVERLAY,      /**< The overlay used for the linger mode. */
		LAYER_BORDER,              /**< The border of the map. */

		LAYER_LAST_LAYER           /**<
		                            * Don't draw to this layer it's a dummy to
		                            * size the vector.
		                            */
		};

	/**
	 * Draw an image at a certain location.
	 * x,y: pixel location on screen to draw the image
	 * image: the image to draw
	 * reverse: if the image should be flipped across the x axis
	 * greyscale: used for instance to give the petrified appearance to a unit image
	 * alpha: the merging to use with the background
	 * blendto: blend to this color using blend_ratio
	 * submerged: the amount of the unit out of 1.0 that is submerged
	 *            (presumably under water) and thus shouldn't be drawn
	 */
	void render_image(int x, int y, const display::tdrawing_layer drawing_layer,
			const map_location& loc, surface image,
			bool hreverse=false, bool greyscale=false,
			fixed_t alpha=ftofxp(1.0), Uint32 blendto=0,
			double blend_ratio=0, double submerged=0.0,bool vreverse =false);

	/**
	 * Draw text on a hex. (0.5, 0.5) is the center.
	 * The font size is adjusted to the zoom factor
	 * and divided by 2 for tiny-gui.
	 */
	void draw_text_in_hex(const map_location& loc,
		const tdrawing_layer layer, const std::string& text, size_t font_size,
		SDL_Color color, double x_in_hex=0.5, double y_in_hex=0.5);

	void draw_text_in_hex2(const map_location& loc,
		const tdrawing_layer layer, const std::string& text,
		size_t font_size, SDL_Color color, int x, int y, fixed_t alpha = 255, bool center = false);

protected:
	/**
	 * In order to render a hex properly it needs to be rendered per row. On
	 * this row several layers need to be drawn at the same time. Mainly the
	 * unit and the background terrain. This is needed since both can spill
	 * in the next hex. The foreground terrain needs to be drawn before to
	 * avoid decapitation a unit.
	 *
	 * In other words:
	 * for every layer
	 *   for every row (starting from the top)
	 *     for every hex in the row
	 *       ...
	 *
	 * this is modified to:
	 * for every layer group
	 *   for every row (starting from the top)
	 *     for every layer in the group
	 *       for every hex in the row
	 *         ...
	 *
	 * * Surfaces are rendered per level in a map.
	 * * Per level the items are rendered per location these locations are
	 *   stored in the drawing order required for units.
	 * * every location has a vector with surfaces, each with its own screen
	 *   coordinate to render at.
	 * * every vector element has a vector with surfaces to render.
	 */
	class drawing_buffer_key
	{
	private:
		unsigned int key_;

		static const tdrawing_layer layer_groups[];
		static const unsigned int max_layer_group;

	public:
		drawing_buffer_key(const map_location &loc, tdrawing_layer layer);

		bool operator<(const drawing_buffer_key &rhs) const { return key_ < rhs.key_; }
	};

	/** Helper structure for rendering the terrains. */
	class tblit
	{
	public:
		tblit(const tdrawing_layer layer, const map_location& loc,
				const int x, const int y, const surface& surf,
				const SDL_Rect& clip)
			: x_(x), y_(y), surf_(1, surf), clip_(clip),
			key_(loc, layer),
			loc_(loc)
		{}

		tblit(const tdrawing_layer layer, const map_location& loc,
				const int x, const int y, const std::vector<surface>& surf,
				const SDL_Rect& clip)
			: x_(x), y_(y), surf_(surf), clip_(clip),
			key_(loc, layer),
			loc_(loc)
		{}

		int x() const { return x_; }
		int y() const { return y_; }
		const std::vector<surface> &surf() const { return surf_; }
		const SDL_Rect &clip() const { return clip_; }

		bool operator<(const tblit &rhs) const { return key_ < rhs.key_; }

		map_location loc_;
	private:
		int x_;                      /**< x screen coordinate to render at. */
		int y_;                      /**< y screen coordinate to render at. */
		std::vector<surface> surf_;  /**< surface(s) to render. */
		SDL_Rect clip_;              /**<
		                              * The clipping area of the source if
		                              * ommitted the entire source is used.
		                              */
		drawing_buffer_key key_;
	};

	typedef std::list<tblit> tdrawing_buffer;
	tdrawing_buffer drawing_buffer_;
	tdrawing_buffer canvas_drawing_buffer_;
	bool to_canvas_;

public:

	/**
	 * Add an item to the drawing buffer. You need to update screen on affected area
	 *
	 * @param layer              The layer to draw on.
	 * @param loc                The hex the image belongs to, needed for the
	 *                           drawing order.
	 * @param blit               The structure to blit.
	 */
	void drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, int x, int y, const surface& surf,
			const SDL_Rect &clip = SDL_Rect());

	void drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, int x, int y,
			const std::vector<surface> &surf,
			const SDL_Rect &clip = SDL_Rect());

	void add_arrow(arrow&);

	void remove_arrow(arrow&);

	/** Called by arrow objects when they change. You should not need to call this directly. */
	void update_arrow(arrow & a);

	/** Draws the drawing_buffer_ and clears it. */
	void drawing_buffer_commit(surface& screen);

	virtual void draw_floating(surface& screen) {}
	virtual void undraw_floating(surface& screen);

protected:
	void create_theme();
	void release_theme();
	virtual SDL_Rect clip_rect_commit() const;

	/**
	 * Strict weak ordering to sort a STL-set of hexes
	 * for drawing using the z-order.
	 * (1000 are just to weight the y compare to x)
	 */
	struct ordered_draw : public std::binary_function<map_location, map_location, bool> {
		bool operator()(map_location a, map_location b) {
			return (a.y*2 + a.x%2) * 1024 + a.x < (b.y*2 + b.x%2) * 1024 + b.x;
		}
	};

	/**
	 * Initiate a redraw.
	 *
	 * Invalidate controls and panels when changed after they have been drawn
	 * initially. Useful for dynamic theme modification.
	 */
	void draw_init();
	void draw_wrap(bool update,bool force);

	virtual bool overlay_road_image(const map_location& loc, std::string& color_mod) const 
	{
		color_mod.clear();
		return false; 
	}

	/** Used to indicate to drawing functions that we are doing a map screenshot */
	bool map_screenshot_;

protected:
	config theme_cfg_;
	SDL_Rect main_map_area_;
	theme::tborder border_;
	const config* theme_current_cfg_;
	gui2::ttheme* theme_;
	
	int min_zoom_;
	int max_zoom_;
	std::map<int, animation*> area_anims_;

	int halo_id_;
	std::map<int, tsurf_buf> haloes_;

	gui2::twidget::torientation orientation_;
	bool original_landscape_;

private:
	/** Count work done for the debug info displayed under fps */
	int invalidated_hexes_;
	int drawn_hexes_;

	double idle_anim_rate_;

	surface map_screenshot_surf_;

	std::vector<boost::function<void(display&)> > redraw_observers_;

	/** Debug flag - overlay x,y coords on tiles */
	bool draw_coordinates_;
	/** Debug flag - overlay terrain codes on tiles */
	bool draw_terrain_codes_;

	typedef std::list<arrow*> arrows_list_t;
	typedef std::map<map_location, arrows_list_t > arrows_map_t;
	/** Maps the list of arrows for each location */
	arrows_map_t arrows_map_;

	controller_base* controller_;
	/**
	 * the tiles invalidated at last redraw,
	 * to simplify the cleaning up of tiles left by units
	 */
	static display * singleton_;
};

class display_lock
{
public:
	display_lock(display& disp)
		: disp_(disp)
	{}
	~display_lock()
	{
		display::set_singleton(&disp_);
	}

private:
	display& disp_;
};

const SDL_Rect& calculate_energy_bar(const surface& surf);
void draw_bar_to_surf(const std::string& image, surface& dst_surf, int x, int y, int size, double filled, const SDL_Color& col, fixed_t alpha, bool vtl);

#endif

