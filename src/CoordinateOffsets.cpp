#include "CoordinateOffsets.hpp"


#include <cassert>
#include <cstdio>


void SendG92Offsets (
    CoordinateOffsets const& i_offsets, GCodeDevice* const o_device) noexcept
{
	static char constexpr kFormat[] = "G92 X%3.3f Y%3.3f Z%3.3f";
	static char constexpr kMaxExpectedNumberString[] = "-000.000";
	static auto constexpr kBufferSize =
	    sizeof (kFormat) + sizeof (kMaxExpectedNumberString) * 3 + 1;


	assert (nullptr != o_device);

	char cmd[ kBufferSize ]{};

	auto const cmd_len = snprintf (
	    cmd, sizeof (cmd), kFormat, i_offsets.x, i_offsets.y, i_offsets.z);

	o_device->scheduleCommand (cmd, cmd_len);
};
