#ifndef SRC_COORDINATEOFFSETS_HPP
#define SRC_COORDINATEOFFSETS_HPP


#include "devices/GCodeDevice.h"

#include "VectorND.hpp"


struct CoordinateOffsets : public Vector3f {
	using Vector3f::Vector3f;
};


void SendG92Offsets (
    CoordinateOffsets const& i_offsets, GCodeDevice* const o_device) noexcept;


#endif // SRC_COORDINATEOFFSETS_HPP
