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
		LoadItemsSettings (menu_conf, kDefaultMenuItems, active_menu_items_);
	}

	if (auto const dro_conf = i_config[ "DRO" ].as< JsonObjectConst > ();
	    !dro_conf.isNull ())
	{
		LoadItemsSettings (dro_conf, kDefaultDroItems, active_dro_items_);
	}
}


void GrblDRO::begin ()
{
	using AddMenuItemFunction = MenuItem (*) (char, GrblDRO&, int16_t&);

	DRO::begin ();

	static etl::flat_map< char, AddMenuItemFunction, kMenuItemCountMax > const
	    menu_item_factory = {
	        {'T',
	         [] (char i_glyph, GrblDRO& io_dro, int16_t& io_id) {
		         return MenuItem::simpleItem (
		             io_id++, i_glyph, [ &dro = io_dro ] (MenuItem&) {
			             Job* job = Job::getJob ();

			             if (job && job->isRunning ())
			             {
				             return;
			             }

			             dro.tool_changed_ = true;

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
			             {
				             return;
			             }

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


void GrblDRO::notification (const DeviceStatusEvent& i_event)
{
	GrblDevice* dev = static_cast< GrblDevice* > (GCodeDevice::getDevice ());

	if (nullptr == dev)
	{
		return;
	}

	auto const can_jog = dev->canJog ();

	if (tool_changed_ || (can_jog != last_can_jog_state_))
	{
		last_can_jog_state_ = can_jog;

		if (tool_changed_ || can_jog)
		{
			tool_changed_ = false;

			target_work_position_ =
			    Vector3f{dev->getX (), dev->getY (), dev->getZ ()};

			target_mach_position_ = Vector3f{
			    target_work_position_.x + dev->getXOfs (),
			    target_work_position_.y + dev->getYOfs (),
			    target_work_position_.z + dev->getZOfs ()};
		}
	}
}


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

	static auto const kStatsLine =
	    kTopY + 3 * (kDroLineHeight + kMachLineHeight);

	static auto const kStatusLineY =
	    Display::u8g2.getHeight () - 2 * kMachLineHeight;


	static auto constexpr ComputeDroLineY = [] (auto&& i_line) {
		return kTopY + kMachLineHeight * (i_line + 1) + kDroLineHeight * i_line;
	};


	static auto constexpr ComputeMachLineY = [] (auto&& i_line) {
		return kTopY + kMachLineHeight * i_line +
		    static_cast< int > (0 < i_line) * kDroLineHeight * i_line;
	};


	GrblDevice* dev = static_cast< GrblDevice* > (GCodeDevice::getDevice ());

	if (nullptr == dev)
	{
		return;
	}

	auto const can_jog = dev->canJog ();

	auto const work_coordinates = can_jog
	    ? target_work_position_
	    : Vector3f{dev->getX (), dev->getY (), dev->getZ ()};

	auto const mach_coordinates = can_jog ? target_mach_position_
	                                      : Vector3f{
	                                            dev->getX () + dev->getXOfs (),
	                                            dev->getY () + dev->getYOfs (),
	                                            dev->getZ () + dev->getZOfs ()};

	U8G2& u8g2 = Display::u8g2;

	{ // Draw DRO coordinates
		u8g2.setFont (kDroFont);

		u8g2.setDrawColor (1);

		if (can_jog)
		{
			u8g2.drawBox (
			    0, ComputeDroLineY (selected_dro_item_) - 1, 8, kDroLineHeight);
		}
		else
		{
			u8g2.drawFrame (
			    0, ComputeDroLineY (selected_dro_item_) - 1, 8, kDroLineHeight);
		}

		u8g2.setDrawColor (2);

		auto dro_line = int{};

		for (auto&& item : active_dro_items_)
		{
			drawAxis (
			    item, work_coordinates[ item ], ComputeDroLineY (dro_line));

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
			DrawMachAxis (
			    mach_coordinates[ item ], ComputeMachLineY (mach_line));

			mach_line++;
		}
	}

	auto const bottom_y =
	    kTopY + active_dro_items_.size () * (kDroLineHeight + kMachLineHeight);

	u8g2.drawHLine (0, kStatsLine - 1, u8g2.getWidth ());

	u8g2.setFont (u8g2_font_5x8_tr);

	const int LEN = 20;
	char      str[ LEN ];

	snprintf (str, LEN, "F%4d S%4d", dev->getFeed (), dev->getSpindleVal ());
	u8g2.drawStr (0, kStatsLine, str);

	float dist = distVal (cDist);

	snprintf (
	    str, LEN, "%c x %.1f", active_dro_items_[ selected_dro_item_ ], dist);

	u8g2.drawStr (0, bottom_y + 7, str);

	const char* stat = dev->isInPanic () ? dev->getLastResponse ().c_str ()
	                                     : dev->getStatus ().c_str ();

	u8g2.drawStr (0, kStatusLineY, stat);
};


void GrblDRO::onButtonPressed (Button i_button, int8_t i_arg)
{
	GCodeDevice* dev = GCodeDevice::getDevice ();

	if ((nullptr == dev) || !dev->canJog () ||
	    !((Button::ENC_UP == i_button) || (Button::ENC_DOWN == i_button)))
	{
		return;
	}

	auto const current_time = millis ();
	auto const jog_distance = distVal (cDist) * i_arg;
	auto const jog_axis     = active_dro_items_[ selected_dro_item_ ];

	auto feed = float{};

	if (lastJogTime != 0)
	{
		feed = jog_distance / (current_time - lastJogTime) * 1000 * 60;
	};

	if (feed < 500)
	{
		feed = 500;
	}

	target_mach_position_[ jog_axis ] += jog_distance;
	target_work_position_[ jog_axis ] += jog_distance;

	lastJogTime = current_time;

	if (!dev->jog (cAxis, jog_distance, static_cast< int > (feed)))
	{
		S_DEBUGF ("Could not schedule jog\n");
	}

	setDirty ();
}


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
