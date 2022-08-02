#include <Arduino.h>
#include <ArduinoJson.h>


#include "DRO.h"


class GrblDRO : public DRO {
public:
	void ApplyConfig (JsonObjectConst i_config) noexcept;

	void begin () override;


protected:
	void drawContents () override;


private:
	String wco_offset_cmd_ = "G10 L20 P1 X0Y0Z0";
};
