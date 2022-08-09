#ifndef SRC_DISCRETE_SWITCH_POTENTIOMETER_HPP
#define SRC_DISCRETE_SWITCH_POTENTIOMETER_HPP


#include <stdint.h>


struct DiscreteSwitchPotentiometerConfig {
	constexpr DiscreteSwitchPotentiometerConfig () = default;

	constexpr DiscreteSwitchPotentiometerConfig (
	    uint16_t i_max_adc_value,
	    uint8_t  i_number_of_steps,
	    uint8_t  i_subrange_clearance_offset)
	    : max_adc_value (i_max_adc_value)
	    , number_of_steps (i_number_of_steps)
	    , subrange_clearance_offset (i_subrange_clearance_offset)
	{
	}


	uint16_t max_adc_value{1200};
	uint8_t  number_of_steps{3};
	uint8_t  subrange_clearance_offset{50};
};


int16_t GetDiscretePotentiomenterPosition (
    DiscreteSwitchPotentiometerConfig const& i_config,
    uint16_t                                 i_adc_value) noexcept;


#endif // SRC_DISCRETE_SWITCH_POTENTIOMETER_HPP
