#ifndef SRC_UI_TOOLTABLE_HPP
#define SRC_UI_TOOLTABLE_HPP


#include <cassert>

#include <functional>

#include <Arduino.h>
#include <ArduinoJson.hpp>

#include <etl/algorithm.h>
#include <etl/flat_map.h>
#include <etl/string.h>
#include <etl/utility.h>
#include <stdio.h>

#include "../CoordinateOffsets.hpp"
#include "Screen.h"


struct Tool {
	etl::string< 10 > name;
	CoordinateOffsets offsets;
};


namespace {
	inline etl::iflat_map< int, Tool, etl::less< int > >::iterator operator+ (
	    etl::iflat_map< int, Tool, etl::less< int > >::iterator i_lhs,
	    unsigned int                                            i_rhs)
	{
		auto result = i_lhs;

		for (int i = 0; i < i_rhs; ++i, ++result)
		{
		}

		return result;
	}
} // namespace


template < size_t KCapacity = 10 >
class ToolTable : public Screen {
public:
	static auto constexpr kCapacity = KCapacity;


	ToolTable () = default;


	void ApplyConfig (JsonObjectConst i_config)
	{
		auto const base_offsets_conf =
		    i_config[ "base_offsets" ].as< JsonObjectConst > ();

		base_offsets_.x = base_offsets_conf[ "x" ].as< float > ();
		base_offsets_.y = base_offsets_conf[ "y" ].as< float > ();
		base_offsets_.z = base_offsets_conf[ "z" ].as< float > ();

		for (auto&& tool_doc : i_config[ "tools" ].as< JsonArrayConst > ())
		{
			auto const tool_obj = tool_doc.as< JsonObjectConst > ();

			auto const tool_id = tool_obj[ "id" ].as< unsigned int > ();

			if (KCapacity < tool_id)
			{
				Serial.print ("Tool ID ");
				Serial.print (tool_id);
				Serial.print (" is greated than the supported maximum value ");
				Serial.println (kCapacity);

				continue;
			}

			if (tools_.contains (tool_id))
			{
				Serial.print ("A tool with ID ");
				Serial.print (tool_id);
				Serial.print (" is already present in table: ");
				Serial.println (tools_[ tool_id ].name.c_str ());

				continue;
			}

			auto& tool = tools_[ tool_id ];

			tool.name = tool_obj[ "name" ].as< String > ().c_str ();

			tool.offsets.x = tool_obj[ "offsets" ][ "x" ].as< float > ();
			tool.offsets.y = tool_obj[ "offsets" ][ "y" ].as< float > ();
			tool.offsets.z = tool_obj[ "offsets" ][ "z" ].as< float > ();
		}
	}


	void SetReturnCallback (std::function< void () > i_return_callback)
	{
		assert (bool (i_return_callback));

		return_callback_ = etl::move (i_return_callback);
	}


	void onShow () override
	{
		selected_line_ = active_tool_line_;
	}


protected:
	void drawContents () override
	{
		U8G2& u8g2 = Display::u8g2;
		u8g2.setDrawColor (1);
		u8g2.setFont (u8g2_font_5x8_tr);

		int line_y = Display::STATUS_BAR_HEIGHT;

		u8g2.drawStr (1, line_y, "Tool select");
		u8g2.drawHLine (0, line_y + kLineHeight - 1, u8g2.getWidth ());

		line_y += kLineHeight;

		DrawMenuLine (0, line_y, -1, nullptr);

		line_y += kLineHeight;

		auto line = int{1};

		for (auto [ id, tool ] : tools_)
		{
			DrawMenuLine (line, line_y, id, tool.name.c_str ());

			line_y += kLineHeight;
			line += 1;
		}
	}


	void onButtonPressed (
	    Button i_button, [[maybe_unused]] int8_t i_arg) override
	{
		switch (i_button)
		{
		default: {
		}
		break;

		case Button::ENC_UP: {
			auto const prev_selected_line = selected_line_;

			selected_line_ = etl::clamp (
			    selected_line_ - 1, 0, static_cast< int > (tools_.size ()));

			setDirty (prev_selected_line != selected_line_);
		}
		break;

		case Button::ENC_DOWN: {
			auto const prev_selected_line = selected_line_;

			selected_line_ = etl::clamp (
			    selected_line_ + 1, 0, static_cast< int > (tools_.size ()));

			setDirty (prev_selected_line != selected_line_);
		}
		break;

		case Button::BT1: {
			return_callback_ ();
		}
		break;

		case Button::BT2: {
			if (active_tool_line_ == selected_line_)
			{
				break;
			}

			auto const prev_tool = active_tool_;

			active_tool_line_ = selected_line_;
			active_tool_      = (0 < selected_line_)
			         ? (tools_.begin () + static_cast< size_t > (active_tool_line_))
                      ->first
			         : kNoToolId;

			if (auto const device = GCodeDevice::getDevice ())
			{
				UpdateToolOffsets (active_tool_, device);
			}

			setDirty ();
		}
		break;
		}
	}


private:
	static auto constexpr kNoToolId     = -1;
	static auto constexpr kLineHeight   = int{10};
	static auto constexpr kMarkRadius   = 2;
	static auto constexpr kMarkDiameter = 2 * kMarkRadius + 1;


	void DrawMenuLine (
	    int const   i_line,
	    const int   i_line_y,
	    int const   i_id,
	    char const* i_name)
	{
		U8G2& u8g2 = Display::u8g2;

		if (selected_line_ == i_line)
		{
			u8g2.setDrawColor (1);
			u8g2.drawBox (0, i_line_y - 1, u8g2.getWidth (), kLineHeight);
			u8g2.setDrawColor (0);
		}
		else
		{
			u8g2.setDrawColor (1);
		}

		if (active_tool_line_ == i_line)
		{
			u8g2.drawDisc (
			    1 + kMarkRadius,
			    i_line_y + kLineHeight / 2 - kMarkRadius + 1,
			    kMarkRadius);
		}
		else
		{
			u8g2.drawCircle (
			    1 + kMarkRadius,
			    i_line_y + kLineHeight / 2 - kMarkRadius + 1,
			    kMarkRadius);
		}

		u8g2.setCursor (kMarkDiameter + 2, i_line_y);

		if (kNoToolId == i_id)
		{
			u8g2.print ("--");
			u8g2.drawStr (kMarkDiameter + 2 + 11, i_line_y, "No tool");
		}
		else
		{
			if (10 > i_id)
			{
				u8g2.print (0);
			}

			u8g2.print (i_id);

			u8g2.drawStr (kMarkDiameter + 2 + 11, i_line_y, i_name);
		}
	}


	void UpdateToolOffsets (
	    int const i_current_tool, GCodeDevice* const o_device)
	{
		assert (nullptr != o_device);

		o_device->scheduleCommand ("G92.1");

		if (kNoToolId == i_current_tool)
		{
			return;
		}

		/*
		  On one hand, those are coordinates and not offsets. On the other hand,
		  new offsets for G92 will be derived from these.
		*/
		auto const current_position = CoordinateOffsets{
		    o_device->getX (), o_device->getY (), o_device->getZ ()};

		SendG92Offsets (
		    current_position + base_offsets_ + tools_[ i_current_tool ].offsets,
		    o_device);
	}

	int selected_line_{0};

	int active_tool_{kNoToolId};
	int active_tool_line_{0};

	CoordinateOffsets base_offsets_;

	etl::flat_map< int, Tool, kCapacity > tools_;

	std::function< void () > return_callback_;
};


#endif // SRC_UI_TOOLTABLE_HPP
