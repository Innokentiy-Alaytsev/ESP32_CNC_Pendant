#include "DRO.h"


#include "../devices/GCodeDevice.h"

#include "../discrete_switch_potentiometer.hpp"
#include "../potentiometers_config.hpp"


DRO::DRO ()
    : nextRefresh (1)
{
}


void DRO::begin ()
{
}


void DRO::loop ()
{
	static auto constexpr kPeriod = 500;

	Screen::loop ();

	auto const current_time = millis ();

	if (refresh_enabled_ && (current_time > nextRefresh))
	{
		nextRefresh = current_time + kPeriod;

		GCodeDevice* dev = GCodeDevice::getDevice ();

		if (dev != nullptr)
		{
			dev->requestStatusUpdate ();
		}
	}
}


void DRO::enableRefresh (bool r)
{
	refresh_enabled_ = r;
	nextRefresh      = r ? millis () : 0;
}


bool DRO::isRefreshEnabled ()
{
	return refresh_enabled_;
}


void DRO::drawContents ()
{
	const int LEN = 20;
	char      str[ LEN ];

	GCodeDevice* dev = GCodeDevice::getDevice ();
	if (dev == nullptr)
		return;

	U8G2& u8g2 = Display::u8g2;

	u8g2.setFont (u8g2_font_7x13B_tr);
	int y = Display::STATUS_BAR_HEIGHT + 3,
	    h = u8g2.getAscent () - u8g2.getDescent () + 3;

	u8g2.setDrawColor (1);
	u8g2.drawBox (0, y + h * (int)cAxis - 1, 8, h);

	u8g2.setDrawColor (2);

	drawAxis ('X', dev->getX (), y);
	y += h;
	drawAxis ('Y', dev->getY (), y);
	y += h;
	drawAxis ('Z', dev->getZ (), y);
	y += h;

	y += 5;
	u8g2.setFont (u8g2_font_nokiafc22_tr);
	float m = distVal (cDist);
	snprintf (str, LEN, m < 1 ? "x%.1f" : "x%.0f", m);
	u8g2.drawStr (0, y, str);
};


void DRO::onPotValueChanged (int i_potentiometer_index, int i_adc_value)
{
	auto const current_position = GetDiscretePotentiomenterPosition (
	    kPotentiometersConfiguration[ i_potentiometer_index ], i_adc_value);

	if (current_position < 0)
	{
		return;
	}

	auto& var = (0 == i_potentiometer_index) ? cAxis : cDist;

	if (current_position != var)
	{
		var = current_position;

		if (0 == i_potentiometer_index)
		{
			lastJogTime = 0;
		}

		setDirty ();
	}
}


void DRO::onButtonPressed (Button bt, int8_t arg)
{
	GCodeDevice* dev = GCodeDevice::getDevice ();

	if (dev == nullptr)
	{
		S_DEBUGF ("device is null\n");
		return;
	}

	switch (bt)
	{
	case Button::ENC_UP:
		[[fallthrough]];
	case Button::ENC_DOWN: {
		if (!dev->canJog ())
		{
			return;
		}

		float f = 0;
		float d = distVal (cDist) * arg;

		if (lastJogTime != 0)
		{
			f = d / (millis () - lastJogTime) * 1000 * 60;
		};

		if (f < 500)
		{
			f = 500;
		}

		bool r = dev->jog ((int)cAxis, d, (int)f);

		lastJogTime = millis ();

		if (!r)
		{
			S_DEBUGF ("Could not schedule jog\n");
		}

		setDirty ();
	}
	break;

	default:
		break;
	}
};


char DRO::axisChar (const JogAxis& a)
{
	switch (a)
	{
	case 0:
		return 'X';
	case 1:
		return 'Y';
	case 2:
		return 'Z';
	}

	log_printf ("Unknown axis\n");

	return 0;
}


float DRO::distVal (const JogDist& a)
{
	switch (a)
	{
	case 0:
		return 0.1;
	case 1:
		return 1;
	case 2:
		return 10;
	}

	log_printf ("Unknown multiplier\n");

	return 1;
}


void DRO::drawAxis (char axis, float v, int y)
{
	const int LEN = 20;
	char      str[ LEN ];

	snprintf (str, LEN, "%c% 8.3f", axis, v);

	Display::u8g2.drawStr (1, y, str);
}
