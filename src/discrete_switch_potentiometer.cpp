#include "discrete_switch_potentiometer.hpp"


#include <etl/algorithm.h>


int16_t GetDiscretePotentiomenterPosition (
    DiscreteSwitchPotentiometerConfig const& i_config,
    uint16_t                                 i_adc_value) noexcept
{
	auto const number_of_steps    = i_config.number_of_steps;
	auto const position_adc_width = i_config.max_adc_value / number_of_steps;
	auto const clearance_offset   = i_config.subrange_clearance_offset;

	auto const maybe_position =
	    etl::min (number_of_steps - 1, i_adc_value / position_adc_width);

	auto const position_range_min =
	    etl::max (0, maybe_position - 1) * position_adc_width +
	    clearance_offset;

	auto const position_range_max =
	    (maybe_position + 1) * position_adc_width - clearance_offset;

	if ((position_range_min <= i_adc_value) &&
	    (i_adc_value <= position_range_max))
	{
		return maybe_position;
	}
	else
	{
		return -1;
	}
}
