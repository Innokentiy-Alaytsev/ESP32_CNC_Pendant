#ifndef SRC_OPTION_SELECTION_HPP
#define SRC_OPTION_SELECTION_HPP


#include <math.h>

#include <etl/array.h>

#include <U8g2lib.h>

#include "VectorND.hpp"

#include "font_info.hpp"


namespace {
	template < class TComponentType >
	constexpr etl::array< Vector2< TComponentType >, 3 > ComputeRadialOffsets (
	    TComponentType const i_radius, int const i_quadrant)
	{
		using VectorType = Vector2< TComponentType >;

		VectorType constexpr kQuadrantSigns[] = {
		    {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};

		if (0 == i_quadrant)
		{
			return {};
		}

		auto const mid_point =
		    static_cast< TComponentType > (sin (M_PI_4) * i_radius);

		auto const quadrant_is_negative = signbit (i_quadrant);

		auto const quadrant_index = abs (i_quadrant) % 5;

		auto const sign = kQuadrantSigns
		    [ quadrant_is_negative ? (4 - quadrant_index)
		                           : (quadrant_index - 1) ];

		return {
		    sign * VectorType{i_radius, 0},
		    sign * VectorType{mid_point, mid_point},
		    sign * VectorType{0, i_radius}};
	}
} // namespace


template <
    int  KOptionsArcRadius,
    int  KOptionsArcQuadrant,
    bool KClockWise,
    class TOptions,
    class TFont >
void DrawOptionSelection (
    Vector2i const& i_options_arc_center,
    TOptions&&      i_options,
    size_t          i_selected_option_index,
    TFont const&    i_font,
    U8G2&           io_u8g2) noexcept
{
	static decltype (U8G2_DRAW_ALL) constexpr kQuadrantArcOptions[] = {
	    U8G2_DRAW_UPPER_RIGHT,
	    U8G2_DRAW_UPPER_LEFT,
	    U8G2_DRAW_LOWER_LEFT,
	    U8G2_DRAW_LOWER_RIGHT};

	static auto const kOptionLineHeight   = ComputeLineHeight (i_font, io_u8g2);
	static auto const kOptionMaxCharWidth = GetMaxCharWidth (i_font, io_u8g2);

	static auto const kOptionArcRadius     = KOptionsArcRadius;
	static auto const kOptionMarkArcRadius = KOptionsArcRadius - 6;

	static auto constexpr kOptionMarkRadius = 2;

	static auto const kOptionRadialOffsets =
	    ComputeRadialOffsets (kOptionArcRadius, -KOptionsArcQuadrant);
	static auto const kOptionMarkRadialOffsets =
	    ComputeRadialOffsets (kOptionMarkArcRadius, -KOptionsArcQuadrant);

	static auto const kOptionFontOffset =
	    Vector2i{-kOptionMaxCharWidth / 2, -kOptionLineHeight / 2};

	io_u8g2.setFont (i_font);
	io_u8g2.setDrawColor (1);

	io_u8g2.drawCircle (i_options_arc_center.x, i_options_arc_center.y, 3);

	io_u8g2.drawCircle (
	    i_options_arc_center.x,
	    i_options_arc_center.y,
	    kOptionMarkArcRadius - kOptionMarkRadius,
	    kQuadrantArcOptions[ KOptionsArcQuadrant - 1 ]);

	io_u8g2.drawDisc (i_options_arc_center.x, i_options_arc_center.y, 1);

	auto option_idx = size_t{};

	for (auto&& option : i_options)
	{
		auto const offset_idx = KClockWise
		    ? option_idx
		    : (kOptionRadialOffsets.size () - 1 - option_idx);
		auto const option_pos =
		    i_options_arc_center + kOptionRadialOffsets[ offset_idx ];

		io_u8g2.setCursor (
		    option_pos.x + kOptionFontOffset.x,
		    option_pos.y + kOptionFontOffset.y);

		io_u8g2.print (option);

		if (option_idx == i_selected_option_index)
		{
			auto const mark_pos =
			    i_options_arc_center + kOptionMarkRadialOffsets[ offset_idx ];

			io_u8g2.drawDisc (mark_pos.x, mark_pos.y, kOptionMarkRadius);
		}

		option_idx++;
	}
};


#endif // SRC_OPTION_SELECTION_HPP
