#include "GrblDRO.h"


#include <etl/algorithm.h>
#include <etl/flat_map.h>

#include "../Job.h"
#include "FileChooser.h"
#include "ui/ToolTable.hpp"

extern FileChooser fileChooser;
extern ToolTable<> tool_table;


void GrblDRO::ApplyConfig (JsonObjectConst i_config) noexcept
{
	static auto constexpr Contains = [] (auto&& i_container, auto&& i_value) {
		return i_container.end () !=
		    etl::find (i_container.begin (), i_container.end (), i_value);
	};


	if (i_config.isNull ())
	{
		return;
	}

	if (auto const wco_offset_cmd_conf = i_config[ "wco_offset_cmd" ];
	    !wco_offset_cmd_conf.isNull ())
	{
		wco_offset_cmd_ = wco_offset_cmd_conf.as< String > ();
	}

	if (auto const menu_conf = i_config[ "menu" ].as< JsonObjectConst > ();
	    !menu_conf.isNull ())
	{
		etl::vector< char, kMenuItemCountMax > disabled_items;

		if (auto const disabled_conf =
		        menu_conf[ "disabled" ].as< JsonArrayConst > ();
		    !disabled_conf.isNull ())
		{
			for (auto&& item : disabled_conf)
			{
				disabled_items.push_back (item.as< String > ()[ 0 ]);
			}
		}

		if (auto const order_conf =
		        menu_conf[ "order" ].as< JsonArrayConst > ();
		    !order_conf.isNull ())
		{

			for (auto&& item : order_conf)
			{
				auto const item_key = item.as< String > ()[ 0 ];

				if (!Contains (disabled_items, item_key))
				{
					active_menu_items.push_back (item_key);
				}
			}
		}

		for (auto&& item : kDefaultMenuItems)
		{
			if (!Contains (disabled_items, item) &&
			    !Contains (active_menu_items, item))
			{
				active_menu_items.push_back (item);
			}
		}
	}
}


void GrblDRO::begin ()
{
	using AddMenuItemFunction = MenuItem (*) (char, GrblDRO&, int16_t&);

	DRO::begin ();

	etl::flat_map< char, AddMenuItemFunction, kMenuItemCountMax >
	    menu_item_factory = {
	        {'T',
	         [] (char i_glyph, GrblDRO&, int16_t& io_id) {
		         return MenuItem::simpleItem (io_id++, i_glyph, [] (MenuItem&) {
			         Job* job = Job::getJob ();

			         if (job && job->isRunning ())
				         return;

			         Display::getDisplay ()->setScreen (&tool_table);
		         });
	         }},
	        {'o',
	         [] (char i_glyph, GrblDRO&, int16_t& io_id) {
		         return MenuItem::simpleItem (io_id++, i_glyph, [] (MenuItem&) {
			         Display::getDisplay ()->setScreen (&fileChooser);
		         });
	         }},
	        {'p',
	         [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem::simpleItem (
		             io_id++, i_glyph, [ &dro = io_dro ] (MenuItem& m) {
			             Job* job = Job::getJob ();
			             if (!job->isRunning ())
				             return;
			             job->setPaused (!job->isPaused ());
			             m.glyph = job->isPaused () ? 'r' : 'p';
			             dro.setDirty (true);
		             });
	         }},
	        {'x',
	         [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem::simpleItem (io_id++, i_glyph, [] (MenuItem&) {
			         GCodeDevice::getDevice ()->reset ();
		         });
	         }},
	        {'u',
	         [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem::simpleItem (
		             io_id++, i_glyph, [ &dro = io_dro ] (MenuItem& m) {
			             dro.enableRefresh (!dro.isRefreshEnabled ());
			             m.glyph = dro.isRefreshEnabled () ? 'u' : 'U';
			             dro.setDirty (true);
		             });
	         }},
	        {'H',
	         [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem::simpleItem (io_id++, i_glyph, [] (MenuItem&) {
			         GCodeDevice::getDevice ()->schedulePriorityCommand ("$H");
		         });
	         }},
	        {'w',
	         [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem::simpleItem (
		             io_id++, i_glyph, [ &dro = io_dro ] (MenuItem&) {
			             GCodeDevice::getDevice ()->scheduleCommand (
			                 dro.wco_offset_cmd_);
			             GCodeDevice::getDevice ()->scheduleCommand ("G54");
		             });
	         }},
	        {'L', [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem{
		             io_id++,
		             i_glyph,
		             true,
		             false,
		             nullptr,
		             [] (MenuItem&) {
			             GCodeDevice::getDevice ()->scheduleCommand ("M3 S1");
		             },
		             [] (MenuItem&) {
			             GCodeDevice::getDevice ()->scheduleCommand ("M5");
		             }};
	         }}};

	auto id = int16_t{};

	for (auto&& key : active_menu_items)
	{
		assert (menu_item_factory.count (key));

		menuItems.push_back (menu_item_factory[ key ](key, *this, id));
	}
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
