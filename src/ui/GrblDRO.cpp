#include "GrblDRO.h"

#include "../Job.h"
#include "FileChooser.h"
#include "ui/ToolTable.hpp"

extern FileChooser fileChooser;
extern ToolTable<> tool_table;


void GrblDRO::ApplyConfig (JsonObjectConst i_config) noexcept
{
	if (i_config.isNull ())
	{
		return;
	}

	if (auto const wco_offset_cmd_conf = i_config[ "wco_offset_cmd" ];
	    !wco_offset_cmd_conf.isNull ())
	{
		wco_offset_cmd_ = wco_offset_cmd_conf.as< String > ();
	}
}


void GrblDRO::begin ()
{
	DRO::begin ();

	auto id = int16_t{};

	menuItems.push_back (MenuItem::simpleItem (id++, 'T', [] (MenuItem&) {
		Job* job = Job::getJob ();

		if (job && job->isRunning ())
			return;

		Display::getDisplay ()->setScreen (&tool_table);
	}));

	menuItems.push_back (MenuItem::simpleItem (id++, 'o', [] (MenuItem&) {
		Display::getDisplay ()->setScreen (&fileChooser);
	}));

	menuItems.push_back (
	    MenuItem::simpleItem (id++, 'p', [ this ] (MenuItem& m) {
		    Job* job = Job::getJob ();
		    if (!job->isRunning ())
			    return;
		    job->setPaused (!job->isPaused ());
		    m.glyph = job->isPaused () ? 'r' : 'p';
		    setDirty (true);
	    }));

	menuItems.push_back (MenuItem::simpleItem (
	    id++, 'x', [] (MenuItem&) { GCodeDevice::getDevice ()->reset (); }));

	menuItems.push_back (
	    MenuItem::simpleItem (id++, 'u', [ this ] (MenuItem& m) {
		    enableRefresh (!isRefreshEnabled ());
		    m.glyph = this->isRefreshEnabled () ? 'u' : 'U';
		    setDirty (true);
	    }));

	menuItems.push_back (MenuItem::simpleItem (id++, 'H', [] (MenuItem&) {
		GCodeDevice::getDevice ()->schedulePriorityCommand ("$H");
	}));

	menuItems.push_back (MenuItem::simpleItem (id++, 'w', [ this ] (MenuItem&) {
		GCodeDevice::getDevice ()->scheduleCommand (wco_offset_cmd_);
		GCodeDevice::getDevice ()->scheduleCommand ("G54");
	}));

	menuItems.push_back (MenuItem{
	    id++,
	    'L',
	    true,
	    false,
	    nullptr,
	    [] (MenuItem&) {
		    GCodeDevice::getDevice ()->scheduleCommand ("M3 S1");
	    },
	    [] (MenuItem&) { GCodeDevice::getDevice ()->scheduleCommand ("M5"); }});
};

void GrblDRO::drawContents ()
{
	const int LEN = 20;
	char      str[ LEN ];

	GrblDevice* dev = static_cast< GrblDevice* > (GCodeDevice::getDevice ());
	if (dev == nullptr)
		return;

	U8G2& u8g2 = Display::u8g2;

	u8g2.setFont (u8g2_font_7x13B_tr);
	int y = Display::STATUS_BAR_HEIGHT + 2,
	    h = u8g2.getAscent () - u8g2.getDescent () + 2;

	// u8g2.drawGlyph(0, y+h*(int)cAxis, '>' );

	u8g2.setDrawColor (1);

	if (dev->canJog ())
		u8g2.drawBox (0, y + h * (int)cAxis - 1, 8, h);
	else
		u8g2.drawFrame (0, y + h * (int)cAxis - 1, 8, h);

	u8g2.setDrawColor (2);

	drawAxis ('X', dev->getX () - dev->getXOfs (), y);
	y += h;
	drawAxis ('Y', dev->getY () - dev->getYOfs (), y);
	y += h;
	drawAxis ('Z', dev->getZ () - dev->getZOfs (), y);
	y += h;

	u8g2.drawHLine (0, y - 1, u8g2.getWidth ());
	if (dev->getXOfs () != 0 || dev->getYOfs () != 0 || dev->getZOfs () != 0)
	{
		drawAxis ('x', dev->getX (), y);
		y += h;
		drawAxis ('y', dev->getY (), y);
		y += h;
		drawAxis ('z', dev->getZ (), y);
		y += h;
	}
	else
	{
		y += 3 * h;
	}

	u8g2.drawHLine (0, y - 1, u8g2.getWidth ());

	u8g2.setFont (u8g2_font_5x8_tr);

	snprintf (str, LEN, "F%4d S%4d", dev->getFeed (), dev->getSpindleVal ());
	u8g2.drawStr (0, y, str);
	y += 7;

	float       m    = distVal (cDist);
	const char* stat = dev->isInPanic () ? dev->getLastResponse ().c_str ()
	                                     : dev->getStatus ().c_str ();

	snprintf (
	    str,
	    LEN,
	    m < 1 ? "%c x%.1f %s" : "%c x%.0f %s",
	    axisChar (cAxis),
	    m,
	    stat);
	u8g2.drawStr (0, y, str);
};
