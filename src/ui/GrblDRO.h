#include <Arduino.h>
#include <ArduinoJson.h>

#include <etl/vector.h>


#include "DRO.h"

#include "../VectorND.hpp"
#include "../devices/GCodeDevice.h"


class GrblDRO : public DRO, public DeviceObserver {
public:
	void ApplyConfig (JsonObjectConst i_config) noexcept;

	void begin () override;

	void notification (const DeviceStatusEvent& i_event) override;


protected:
	void drawContents () override;

	void onButtonPressed (Button i_button, int8_t i_arg) override;
	void onPotValueChanged (int i_pot, int i_value) override;


private:
	static size_t constexpr kMenuItemCountMax = 10;
	static size_t constexpr kDroItemCountMax  = 3;

	static inline etl::vector< char, kMenuItemCountMax > const
	    kDefaultMenuItems = {'T', 'o', 'p', 'u', 'H', 'w', 'L'};

	static inline etl::vector< char, kDroItemCountMax > const kDefaultDroItems =
	    {'X', 'Y', 'Z'};

	String wco_offset_cmd_ = "G10 L20 P1 X0Y0Z0";
	etl::vector< char, kMenuItemCountMax > active_menu_items_;
	etl::vector< char, kDroItemCountMax >  active_dro_items_;

	int selected_dro_item_{0};

	bool last_can_jog_state_{false};
	bool tool_changed_{false};

	Vector3f target_mach_position_;
	Vector3f target_work_position_;
};
