#ifndef SRC_COORDINATEOFFSETS_HPP
#define SRC_COORDINATEOFFSETS_HPP


#include "devices/GCodeDevice.h"


struct CoordinateOffsets {
	constexpr CoordinateOffsets () = default;

	constexpr CoordinateOffsets (float i_x, float i_y, float i_z)
	    : x{i_x}
	    , y{i_y}
	    , z{i_z}
	{
	}


	constexpr CoordinateOffsets operator- () const noexcept
	{
		return CoordinateOffsets{-x, -y, -z};
	}


	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};


constexpr CoordinateOffsets operator- (
    CoordinateOffsets const& i_lhs, CoordinateOffsets const& i_rhs) noexcept
{
	return CoordinateOffsets{
	    i_lhs.x - i_rhs.x, i_lhs.y - i_rhs.y, i_lhs.z - i_rhs.z};
}


constexpr CoordinateOffsets operator+ (
    CoordinateOffsets const& i_lhs, CoordinateOffsets const& i_rhs) noexcept
{
	return CoordinateOffsets{
	    i_lhs.x + i_rhs.x, i_lhs.y + i_rhs.y, i_lhs.z + i_rhs.z};
}


void SendG92Offsets (
    CoordinateOffsets const& i_offsets, GCodeDevice* const o_device) noexcept;


#endif // SRC_COORDINATEOFFSETS_HPP
