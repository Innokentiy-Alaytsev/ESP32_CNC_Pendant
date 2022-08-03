#include "GrblDRO.h"


#include <etl/algorithm.h>
#include <etl/flat_map.h>
#include <etl/type_traits.h>

#include "../Job.h"
#include "FileChooser.h"
#include "ui/ToolTable.hpp"

extern FileChooser fileChooser;
extern ToolTable<> tool_table;

namespace {
	template < class TContainer, class TValue >
	constexpr bool Contains (TContainer&& i_container, TValue&& i_value)
	{
		return i_container.end () !=
		    etl::find (i_container.begin (), i_container.end (), i_value);
	};


	template < class TConfig, class TDefaultItems, class TActiveItems >
	constexpr void LoadItemsSettings (
	    TConfig&&       i_config,
	    TDefaultItems&& i_default_items,
	    TActiveItems&   o_active_items)
	{
		etl::vector< char, etl::remove_reference_t< TDefaultItems >::MAX_SIZE >
		    disabled_items;

		if (auto const disabled_conf =
		        i_config[ "disabled" ].template as< JsonArrayConst > ();
		    !disabled_conf.isNull ())
		{
			for (auto&& item : disabled_conf)
			{
				disabled_items.push_back (item.template as< String > ()[ 0 ]);
			}
		}

		if (auto const order_conf =
		        i_config[ "order" ].template as< JsonArrayConst > ();
		    !order_conf.isNull ())
		{

			for (auto&& item : order_conf)
			{
				auto const item_key = item.template as< String > ()[ 0 ];

				if (!Contains (disabled_items, item_key))
				{
					o_active_items.push_back (item_key);
				}
			}
		}

		for (auto&& item : i_default_items)
		{
			if (!Contains (disabled_items, item) &&
			    !Contains (o_active_items, item))
			{
				o_active_items.push_back (item);
			}
		}
	};
} // namespace


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

	if (auto const menu_conf = i_config[ "menu" ].as< JsonObjectConst > ();
	    !menu_conf.isNull ())
	{
		LoadItemsSettings (menu_conf, kDefaultMenuItems, active_menu_items);
	}

	if (auto const dro_conf = i_config[ "DRO" ].as< JsonObjectConst > ();
	    !dro_conf.isNull ())
	{
		LoadItemsSettings (dro_conf, kDefaultDroItems, active_dro_items);
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

	u8g2.setDrawColor (1);

	if (dev->canJog ())
		u8g2.drawBox (0, y + h * static_cast< int > (selected_dro_item_), 8, h);
	else
		u8g2.drawFrame (
		    0, y + h * static_cast< int > (selected_dro_item_), 8, h);

	u8g2.setDrawColor (2);

	/*
	  Not using a map this time since the DRO items are fer in number and are
	  repeated (with and without offsets). Another reason is that items would
	  have to be stored in GrblDRO object permanently.
	*/
	for (auto&& item : active_dro_items)
	{
		switch (item)
		{
		default: {
			continue;
		}
		break;

		case 'X': {
			drawAxis ('X', dev->getX () - dev->getXOfs (), y);
		}
		break;

		case 'Y': {
			drawAxis ('Y', dev->getY () - dev->getYOfs (), y);
		}
		break;

		case 'Z': {
			drawAxis ('Z', dev->getZ () - dev->getZOfs (), y);
		}
		break;
		}

		y += h;
	}

	u8g2.drawHLine (0, y - 1, u8g2.getWidth ());
	if (dev->getXOfs () != 0 || dev->getYOfs () != 0 || dev->getZOfs () != 0)
	{
		for (auto&& item : active_dro_items)
		{
			switch (item)
			{
			default: {
				continue;
			}
			break;

			case 'X': {
				drawAxis ('x', dev->getX (), y);
			}
			break;

			case 'Y': {
				drawAxis ('y', dev->getY (), y);
			}
			break;

			case 'Z': {
				drawAxis ('z', dev->getZ (), y);
			}
			break;
			}

			y += h;
		}
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
	    active_dro_items[ selected_dro_item_ ],
	    m,
	    stat);
	u8g2.drawStr (0, y, str);
};


void GrblDRO::onPotValueChanged (int i_pot, int i_value)
{
	DRO::onPotValueChanged (i_pot, i_value);

	selected_dro_item_ = etl::clamp (
	    cAxis, 0, static_cast< int > (active_dro_items.size ()) - 1);

	cAxis = static_cast< JogAxis > (etl::distance (
	    kDefaultDroItems.begin (),
	    etl::find (
	        kDefaultDroItems.begin (),
	        kDefaultDroItems.end (),
	        active_dro_items[ selected_dro_item_ ])));
}
