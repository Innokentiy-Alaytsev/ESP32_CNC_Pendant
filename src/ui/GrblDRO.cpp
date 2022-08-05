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
	if (auto const wco_offset_cmd_conf = i_config[ "wco_offset_cmd" ];
	    !wco_offset_cmd_conf.isNull ())
	{
		wco_offset_cmd_ = wco_offset_cmd_conf.as< String > ();
	}

	LoadItemsSettings (
	    i_config[ "menu" ].as< JsonObjectConst > (),
	    kDefaultMenuItems,
	    active_menu_items_);

	LoadItemsSettings (
	    i_config[ "DRO" ].as< JsonObjectConst > (),
	    kDefaultDroItems,
	    active_dro_items_);
}


void GrblDRO::begin ()
{
	using AddMenuItemFunction = MenuItem (*) (char, GrblDRO&, int16_t&);

	DRO::begin ();

	static etl::flat_map< char, AddMenuItemFunction, kMenuItemCountMax > const
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

	for (auto&& key : active_menu_items_)
	{
		assert (menu_item_factory.count (key));

		menuItems.push_back (menu_item_factory.at (key) (key, *this, id));
	}
};


void GrblDRO::drawContents ()
{
	static auto constexpr ComputeLineHeight = [] (auto&& i_font,
	                                              auto&& io_u8g2) {
		io_u8g2.setFont (i_font);

		return io_u8g2.getAscent () - io_u8g2.getDescent () + 2;
	};


	static constexpr auto& kDroFont  = u8g2_font_7x13B_tr;
	static constexpr auto& kMachFont = u8g2_font_5x8_tr;

	static auto const kDroLineHeight =
	    ComputeLineHeight (kDroFont, Display::u8g2);
	static auto const kMachLineHeight =
	    ComputeLineHeight (kMachFont, Display::u8g2);

	static auto constexpr kTopY = Display::STATUS_BAR_HEIGHT + 2;


	static auto constexpr ComputeDroLineY = [] (auto&& i_line) {
		return kTopY + kMachLineHeight * (i_line + 1) + kDroLineHeight * i_line;
	};


	static auto constexpr ComputeMachLineY = [] (auto&& i_line) {
		return kTopY + kMachLineHeight * i_line +
		    static_cast< int > (0 < i_line) * kDroLineHeight * i_line;
	};


	GrblDevice* dev = static_cast< GrblDevice* > (GCodeDevice::getDevice ());

	if (dev == nullptr)
	{
		return;
	}

	U8G2& u8g2 = Display::u8g2;

	{ // Draw DRO coordinates
		u8g2.setFont (kDroFont);

		u8g2.setDrawColor (1);

		if (dev->canJog ())
		{
			u8g2.drawBox (
			    0, ComputeDroLineY (selected_dro_item_), 8, kDroLineHeight);
		}
		else
		{
			u8g2.drawFrame (
			    0, ComputeDroLineY (selected_dro_item_), 8, kDroLineHeight);
		}

		u8g2.setDrawColor (2);

		auto dro_line = int{};

		/*
		  Not using a map this time since the DRO items are few in number and
		  are repeated (with and without offsets). Another reason is that items
		  would have to be stored in GrblDRO object permanently.
		*/
		for (auto&& item : active_dro_items_)
		{
			switch (item)
			{
			default: {
				continue;
			}
			break;

			case 'X': {
				drawAxis ('X', dev->getX (), ComputeDroLineY (dro_line));
			}
			break;

			case 'Y': {
				drawAxis ('Y', dev->getY (), ComputeDroLineY (dro_line));
			}
			break;

			case 'Z': {
				drawAxis ('Z', dev->getZ (), ComputeDroLineY (dro_line));
			}
			break;
			}

			dro_line++;
		}
	}

	{ // Draw machine coordinates
		u8g2.setFont (kMachFont);

		auto mach_line = int{};

		static auto constexpr DrawMachAxis = [] (auto&& i_value,
		                                         auto&& i_line_y) {
			char str[ 20 ]{};

			snprintf (str, sizeof (str), "\t%*8.3f", 8, i_value);
			Display::u8g2.drawStr (1, i_line_y, str);
		};

		for (auto&& item : active_dro_items_)
		{
			switch (item)
			{
			default: {
				continue;
			}
			break;

			case 'X': {
				DrawMachAxis (
				    dev->getX () + dev->getXOfs (),
				    ComputeMachLineY (mach_line));
			}
			break;

			case 'Y': {
				DrawMachAxis (
				    dev->getY () + dev->getYOfs (),
				    ComputeMachLineY (mach_line));
			}
			break;

			case 'Z': {
				DrawMachAxis (
				    dev->getZ () + dev->getZOfs (),
				    ComputeMachLineY (mach_line));
			}
			break;
			}

			mach_line++;
		}
	}

	auto const bottom_y =
	    kTopY + active_dro_items_.size () * (kDroLineHeight + kMachLineHeight);

	u8g2.drawHLine (0, bottom_y - 1, u8g2.getWidth ());

	u8g2.setFont (u8g2_font_5x8_tr);

	const int LEN = 20;
	char      str[ LEN ];

	snprintf (str, LEN, "F%4d S%4d", dev->getFeed (), dev->getSpindleVal ());
	u8g2.drawStr (0, bottom_y, str);

	float dist = distVal (cDist);

	snprintf (
	    str, LEN, "%c x %.1f", active_dro_items_[ selected_dro_item_ ], dist);

	u8g2.drawStr (0, bottom_y + 7, str);

	const char* stat = dev->isInPanic () ? dev->getLastResponse ().c_str ()
	                                     : dev->getStatus ().c_str ();

	u8g2.drawStr (0, bottom_y + 14, stat);
};


void GrblDRO::onPotValueChanged (int i_pot, int i_value)
{
	DRO::onPotValueChanged (i_pot, i_value);

	selected_dro_item_ = etl::clamp (
	    cAxis, 0, static_cast< int > (active_dro_items_.size ()) - 1);

	cAxis = static_cast< JogAxis > (etl::distance (
	    kDefaultDroItems.begin (),
	    etl::find (
	        kDefaultDroItems.begin (),
	        kDefaultDroItems.end (),
	        active_dro_items_[ selected_dro_item_ ])));
}
