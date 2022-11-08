#include "SpindleControl.hpp"


#include <stdio.h>

#include <etl/algorithm.h>
#include <etl/array.h>
#include <etl/flat_map.h>
#include <etl/type_traits.h>

#include "../devices/GrblDevice.hpp"

#include "../Job.h"

#include "../font_info.hpp"
#include "../option_selection.hpp"

#include "../discrete_switch_potentiometer.hpp"
#include "../potentiometers_config.hpp"


void SpindleControl::ApplyConfig (JsonObjectConst i_config) noexcept
{
	if (auto const max_rpm_conf = i_config[ "max_rpm" ];
	    !max_rpm_conf.isNull ())
	{
		max_rpm_ = max_rpm_conf.as< uint16_t > ();
	}

	if (auto const rpm_step_values_conf =
	        i_config[ "rpm_step_values" ].as< JsonArrayConst > ();
	    !rpm_step_values_conf.isNull () &&
	    (3 == rpm_step_values_conf.size ()) &&
	    rpm_step_values_conf[ 0 ].is< uint16_t > () &&
	    rpm_step_values_conf[ 1 ].is< uint16_t > () &&
	    rpm_step_values_conf[ 2 ].is< uint16_t > ())
	{
		rpm_change_step_values_[ 0 ] = rpm_step_values_conf[ 0 ].as< float > ();
		rpm_change_step_values_[ 1 ] = rpm_step_values_conf[ 1 ].as< float > ();
		rpm_change_step_values_[ 2 ] = rpm_step_values_conf[ 2 ].as< float > ();
	}
}


void SpindleControl::SetReturnCallback (
    std::function< void () > i_return_callback)
{
	assert (bool (i_return_callback));

	return_callback_ = etl::move (i_return_callback);
}


void SpindleControl::notification (const DeviceStatusEvent& i_event)
{
	if (auto const device =
	        static_cast< GrblDevice* > (GCodeDevice::getDevice ()))
	{
		auto const prev_actual_rpm = mach_rpm_;

		mach_rpm_ = device->getSpindleVal ();

		setDirty (prev_actual_rpm != mach_rpm_);
	}
}


void SpindleControl::drawContents ()
{
	static constexpr auto& kMainFont   = u8g2_font_8x13B_tr;
	static constexpr auto& kMachFont   = u8g2_font_5x7_tr;
	static constexpr auto& kOptionFont = u8g2_font_4x6_tr;

	static auto const kMainLineHeight =
	    ComputeLineHeight (kMainFont, Display::u8g2);
	static auto const kMachLineHeight =
	    ComputeLineHeight (kMachFont, Display::u8g2);

	static auto constexpr kTopY = Display::STATUS_BAR_HEIGHT + 1;

	static auto const kStatsLine =
	    kTopY + 3 * (kMainLineHeight + kMachLineHeight);

	static auto const kStatusLineY =
	    Display::u8g2.getHeight () - kMachLineHeight;


	static auto constexpr ComputeMainLineY = [] (auto&& i_line) {
		return kTopY + kMachLineHeight * (i_line + 1) +
		    kMainLineHeight * i_line;
	};


	static auto constexpr ComputeMachLineY = [] (auto&& i_line) {
		return kTopY + kMachLineHeight * i_line +
		    static_cast< int > (0 < i_line) * kMainLineHeight * i_line;
	};


	GrblDevice* device = static_cast< GrblDevice* > (GCodeDevice::getDevice ());

	if (nullptr == device)
	{
		return;
	}

	auto const can_jog = device->canJog ();

	auto const main_rpm = can_jog ? target_rpm_ : mach_rpm_;

	U8G2& u8g2 = Display::u8g2;

	u8g2.setDrawColor (1);

	{ // Draw spindle RPM
		static char constexpr kFormat[]               = "S%*d";
		static char constexpr kMaxExpectedRpmString[] = "00000";
		static auto constexpr kBufferSize =
		    sizeof (kFormat) + sizeof (kMaxExpectedRpmString) + 1;

		u8g2.setFont (kMainFont);

		char str[ kBufferSize ]{};

		snprintf (str, sizeof (str), kFormat, 6, main_rpm);

		u8g2.drawStr (5, ComputeMainLineY (0), str);
	}

	{ // Draw active command display
		static constexpr auto& kCommandFont = u8g2_font_7x13B_tr;

		static auto const kCommandLineHeight =
		    ComputeLineHeight (kCommandFont, Display::u8g2);
		static auto const kCommandMaxCharWidth =
		    GetMaxCharWidth (kCommandFont, Display::u8g2);

		static auto const kCommandWidth = 2 * kCommandMaxCharWidth;

		static auto const kCommandLine = ComputeMachLineY (2);

		static auto constexpr kCommandBoxMargin = 5;

		static auto const kCommandBoxSize = Vector2i{
		    kCommandWidth + 2 * kCommandBoxMargin,
		    kCommandLineHeight + 2 * kCommandBoxMargin};

		static auto constexpr ComputeCommandPositionX = [] (auto&& i_u8g2,
		                                                    auto&& i_cmd_idx) {
			auto const segment_width = i_u8g2.getWidth () / 3;

			auto const segment_center =
			    segment_width * i_cmd_idx + segment_width / 2;

			return segment_center - kCommandWidth / 2;
		};

		u8g2.setFont (kCommandFont);

		auto cmd_idx = size_t{};

		for (auto&& command : kSpindleControlCommands)
		{
			auto const cmd_x = ComputeCommandPositionX (u8g2, cmd_idx);

			u8g2.setDrawColor (1);

			if (cmd_idx == active_command_)
			{
				u8g2.drawBox (
				    cmd_x - kCommandBoxMargin + 1,
				    kCommandLine - kCommandBoxMargin,
				    kCommandBoxSize.x,
				    kCommandBoxSize.y);
			}
			else
			{
				u8g2.drawFrame (
				    cmd_x - kCommandBoxMargin + 1,
				    kCommandLine - kCommandBoxMargin,
				    kCommandBoxSize.x,
				    kCommandBoxSize.y);
			}

			u8g2.setDrawColor (2);

			u8g2.drawStr (cmd_x, kCommandLine, command.c_str ());

			cmd_idx++;
		}
	}

	{ // Draw machine spindle RPM
		static char constexpr kFormat[]               = "%*d";
		static char constexpr kMaxExpectedRpmString[] = "00000";
		static auto constexpr kBufferSize =
		    sizeof (kFormat) + sizeof (kMaxExpectedRpmString) + 1;

		u8g2.setFont (kMachFont);

		char str[ kBufferSize ]{};

		snprintf (
		    str, sizeof (str), kFormat, 8, static_cast< int > (mach_rpm_));

		u8g2.drawStr (1, ComputeMachLineY (0), str);
	}

	{ // Draw options selection
		static auto constexpr kOptionArcRadius = 18;

		static auto const kOptionsBottom =
		    kStatsLine + kOptionArcRadius + kMachLineHeight + 4;

		DrawOptionSelection< kOptionArcRadius, 2, true > (
		    Vector2i{u8g2.getWidth () / 2 - 10, kOptionsBottom},
		    kSpindleControlCommands,
		    selected_command_,
		    kOptionFont,
		    u8g2);

		DrawOptionSelection< kOptionArcRadius, 1, false > (
		    Vector2i{u8g2.getWidth () / 2, kOptionsBottom},
		    rpm_change_step_values_,
		    selected_step_,
		    kOptionFont,
		    u8g2);
	}

	u8g2.setFont (u8g2_font_4x6_tr);

	const char* stat = device->isInPanic ()
	    ? device->getLastResponse ().c_str ()
	    : device->getStatus ().c_str ();

	u8g2.drawStr (0, kStatusLineY, stat);
};


void SpindleControl::onButtonPressed (Button i_button, int8_t i_arg)
{
	if (Button::BT1 == i_button)
	{
		return_callback_ ();
	}

	GCodeDevice* device = GCodeDevice::getDevice ();

	if ((nullptr == device) || !device->canJog ())
	{
		return;
	}

	auto const prev_target_rpm = target_rpm_;

	switch (i_button)
	{
	default: {
	}
	break;

	case Button::ENC_DOWN:
		[[fallthrough]];
	case Button::ENC_UP: {
		target_rpm_ = static_cast< decltype (target_rpm_) > (etl::clamp (
		    static_cast< int32_t > (target_rpm_) +
		        rpm_change_step_values_[ selected_step_ ] * i_arg,
		    static_cast< int32_t > (0),
		    static_cast< int32_t > (max_rpm_)));
	}
	break;

	case Button::BT2: {
		if (selected_command_ != active_command_)
		{
			device->scheduleCommand (
			    kSpindleControlCommands[ selected_command_ ]);

			active_command_ = selected_command_;

			setDirty ();
		}
	}
	break;
	}

	if (prev_target_rpm != target_rpm_)
	{
		static char constexpr kFormat[]               = "S%d";
		static char constexpr kMaxExpectedRpmString[] = "00000";
		static auto constexpr kBufferSize =
		    sizeof (kFormat) + sizeof (kMaxExpectedRpmString) + 1;

		char cmd[ kBufferSize ]{};

		auto const cmd_len = snprintf (cmd, sizeof (cmd), kFormat, target_rpm_);

		active_command_ = selected_command_;

		device->scheduleCommand (cmd, cmd_len);

		setDirty ();
	}
}


void SpindleControl::onPotValueChanged (
    int i_potentiometer_index, int i_adc_value)
{
	auto const current_position = GetDiscretePotentiomenterPosition (
	    kPotentiometersConfiguration[ i_potentiometer_index ], i_adc_value);

	if (current_position < 0)
	{
		return;
	}

	auto& var =
	    (0 == i_potentiometer_index) ? selected_command_ : selected_step_;

	if (current_position != var)
	{
		var = current_position;

		setDirty ();
	}
}
