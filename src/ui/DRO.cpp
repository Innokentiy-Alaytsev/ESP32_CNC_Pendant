#include "DRO.h"


#include <etl/algorithm.h>

#include "../devices/GCodeDevice.h"


DRO::DRO ()
    : nextRefresh (1)
{
}


void DRO::begin ()
{
}


void DRO::loop ()
{
	Screen::loop ();

	if (nextRefresh != 0 && millis () > nextRefresh)
	{
		nextRefresh = millis () + 500;

		GCodeDevice* dev = GCodeDevice::getDevice ();

		if (dev != nullptr)
		{
			dev->requestStatusUpdate ();
		}
	}
}


void DRO::enableRefresh (bool r)
{
	nextRefresh = r ? millis () : 0;
}


bool DRO::isRefreshEnabled ()
{
	return nextRefresh != 0;
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


struct POT_CFG {
	int MX;
	int N;
	int D;
};


static constexpr POT_CFG POTS_CFG[]{{1200, 3, 50}, {1200, 3, 50}};


void DRO::onPotValueChanged (int pot, int i_adc_value)
{
	const POT_CFG& p                  = POTS_CFG[ pot ];
	const int      position_adc_width = p.MX / p.N;
	const int      clearance_offset   = POTS_CFG[ pot ].D;
	int&           var                = pot == 0 ? cAxis : cDist;
	bool           ch                 = false;

	auto const maybe_position =
	    etl::min (p.N - 1, i_adc_value / position_adc_width);

	auto const position_range_min =
	    etl::max (0, maybe_position - 1) * position_adc_width +
	    clearance_offset;

	auto const position_range_max =
	    (maybe_position + 1) * position_adc_width - clearance_offset;

	if ((position_range_min <= i_adc_value) &&
	    (i_adc_value <= position_range_max))
	{
		var = maybe_position;

		ch = true;
	}

	if (ch && pot == 0)
	{
		lastJogTime = 0;
	}

	if (ch)
	{
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
