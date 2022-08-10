#ifndef SRC_UI_SPINDLECONTROL_HPP
#define SRC_UI_SPINDLECONTROL_HPP


#include <functional>

#include <Arduino.h>
#include <ArduinoJson.h>

#include <etl/vector.h>


#include "Dro.h"

#include "../VectorND.hpp"
#include "../devices/GCodeDevice.h"


/*
  Deriving from DRO allows reusing automatic periodic update. Re-implementing it
  here makes little to no sense.
*/
class SpindleControl : public DRO, public DeviceObserver {
public:
	void ApplyConfig (JsonObjectConst i_config) noexcept;

	void SetReturnCallback (std::function< void () > i_return_callback);

	void notification (const DeviceStatusEvent& i_event) override;


protected:
	void drawContents () override;

	void onButtonPressed (Button i_button, int8_t i_arg) override;
	void onPotValueChanged (int i_potentiometer_index, int i_value) override;


private:
	enum SpindleCommand { kM4, kM5, kM3 };

	static size_t constexpr kSpindleControlCommandsCount = 3;
	static size_t constexpr kRpmChangeStepCount          = 3;

	static inline etl::vector< String, kSpindleControlCommandsCount > const
	    kSpindleControlCommands = {"M4", "M5", "M3"};

	static inline etl::vector< uint16_t, kRpmChangeStepCount > const
	    kDefaultRpmChangeStepValues = {10, 50, 100};

	etl::vector< uint16_t, kRpmChangeStepCount > rpm_change_step_values_ =
	    kDefaultRpmChangeStepValues;

	int selected_step_{0};
	int selected_command_{SpindleCommand::kM5};
	int active_command_{SpindleCommand::kM5};

	uint16_t target_rpm_{0};
	uint16_t mach_rpm_{0};

	uint16_t max_rpm_{2700};

	std::function< void () > return_callback_;
};


#endif // SRC_UI_SPINDLECONTROL_HPP
