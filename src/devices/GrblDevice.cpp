#include "GrblDevice.hpp"


bool GrblDevice::jog (uint8_t axis, float dist, int feed)
{
	constexpr static char AXIS[] = {'X', 'Y', 'Z'};
	char                  msg[ 81 ];

	snprintf (msg, 81, "$J=G91 F%d %c%.3f", feed, AXIS[ axis ], dist);

	return scheduleCommand (msg, strlen (msg));
}


bool GrblDevice::canJog ()
{
	return status == "Idle" || status == "Jog";
}


bool GrblDevice::isCmdRealtime (char* data, size_t len)
{
	if (len != 1)
	{
		return false;
	}

	unsigned char c = static_cast< unsigned char > (data[ 0 ]);

	switch (c)
	{
	case '?':
		[[fallthrough]]; // status
	case '~':
		[[fallthrough]]; // cycle start/stop
	case '!':
		[[fallthrough]]; // feedhold
	case 0x18:
		[[fallthrough]]; // ^x, reset
	case 0x84:
		[[fallthrough]]; // door
	case 0x85:
		[[fallthrough]]; // jog cancel
	case 0x9E:
		[[fallthrough]]; // toggle spindle
	case 0xA0:
		[[fallthrough]]; // toggle flood coolant
	case 0xA1: {         // toggle mist coolant
		return true;
	}
	default: {
		// feed override, rapid override, spindle override
		if (c >= 0x90 && c <= 0x9D)
		{
			return true;
		}
		return false;
	}
	}
}


void GrblDevice::trySendCommand ()
{
	if (isCmdRealtime (curUnsentPriorityCmd, curUnsentPriorityCmdLen))
	{
		printerSerial->write (curUnsentPriorityCmd, curUnsentPriorityCmdLen);

		GD_DEBUGF (
		    "<  (f%3d,%3d) '%c' RT\n",
		    sentCounter->getFreeLines (),
		    sentCounter->getFreeBytes (),
		    curUnsentPriorityCmd[ 0 ]);

		curUnsentPriorityCmdLen = 0;

		return;
	}

	char* cmd = (curUnsentPriorityCmdLen != 0) ? &curUnsentPriorityCmd[ 0 ]
	                                           : &curUnsentCmd[ 0 ];
	auto& len = (curUnsentPriorityCmdLen != 0) ? curUnsentPriorityCmdLen
	                                           : curUnsentCmdLen;

	if (sentCounter->canPush (len))
	{
		sentCounter->push (cmd, len);

		printerSerial->write (cmd, len);

		printerSerial->print ('\n');

		GD_DEBUGF (
		    "<  (f%3d,%3d) '%s' (%d)\n",
		    sentCounter->getFreeLines (),
		    sentCounter->getFreeBytes (),
		    cmd,
		    len);

		len = 0;
	}
}


void GrblDevice::tryParseResponse (char* i_resp, size_t i_length)
{
	auto const response = etl::string_view{i_resp, i_length};

	if (response.starts_with ("ok"))
	{
		sentQueue.pop ();

		connected = true;
		panic     = false;
	}
	else if (response.starts_with ("error") || response.starts_with ("ALARM:"))
	{
		sentQueue.pop ();

		panic = true;

		GD_DEBUGF ("ERR '%s'\n", i_resp);

		notify_observers (DeviceStatusEvent{1});

		lastResponse = i_resp;
	}
	else if (response.starts_with ('<'))
	{
		parseGrblStatus (response);
	}
	else if (response.starts_with ("[MSG:"))
	{
		GD_DEBUGF ("Msg '%s'\n", i_resp);

		lastResponse = String{
		    response.data () + response.find_first_of (':') + 1,
		    response.size ()};
	}

	GD_DEBUGF (
	    " > (f%3d,%3d) '%s' \n",
	    sentQueue.getFreeLines (),
	    sentQueue.getFreeBytes (),
	    resp);
}


void GrblDevice::parseGrblStatus (etl::string_view i_status_string)
{
	static auto constexpr kSeparator = '|';


	struct Coordinates {
		float x{};
		float y{};
		float z{};
	};


	static auto constexpr ConsumeCoordinatesTriplet = [] (auto&& i_string) {
		Coordinates coordinates;

		coordinates.x = atof (i_string.data ());

		i_string.remove_prefix (i_string.find_first_of (',') + 1);

		coordinates.y = atof (i_string.data ());

		i_string.remove_prefix (i_string.find_first_of (',') + 1);

		coordinates.z = atof (i_string.data ());

		return coordinates;
	};


	//<Idle|MPos:9.800,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
	//<Idle|MPos:9.800,0.000,0.000|FS:0,0|Ov:100,100,100>

	// Remove '<' at the beginning of the status string
	i_status_string.remove_prefix (1);

	if (auto const status_separator_pos =
	        i_status_string.find_first_of (kSeparator);
	    etl::string_view::npos != status_separator_pos)
	{
		status = String{i_status_string.data (), status_separator_pos};

		i_status_string.remove_prefix (status_separator_pos + 1);
	}
	else
	{
		return;
	}

	// MPos:0.000,0.000,0.000
	if (auto const mpos_separator_pos =
	        i_status_string.find_first_of (kSeparator);
	    etl::string_view::npos != mpos_separator_pos)
	{
		auto mpos_view = i_status_string;

		mpos_view.remove_prefix (sizeof ("MPos:") - 1);

		auto const coordinates = ConsumeCoordinatesTriplet (mpos_view);

		x = coordinates.x;
		y = coordinates.y;
		z = coordinates.z;

		i_status_string.remove_prefix (mpos_separator_pos + 1);
	}
	else
	{
		return;
	}

	input_pin_state_.clear ();

	enum InfoMask {
		kSpindleAndFeed = 0b001,
		kWco            = 0b010,
		kInputPinState  = 0b100
	};

	auto parsed_info = int{};

	input_pin_state_.clear ();

	for (auto next_separator_pos = i_status_string.find_first_of (kSeparator);
	     etl::string_view::npos != next_separator_pos;
	     i_status_string.remove_prefix (next_separator_pos + 1),
	          next_separator_pos = i_status_string.find_first_of (kSeparator))
	{
		auto current_info_view = i_status_string;

		if (!(parsed_info & InfoMask::kSpindleAndFeed) &&
		    (current_info_view.starts_with ("FS:") ||
		     current_info_view.starts_with ("F:")))
		{
			if ('S' == current_info_view[ 1 ])
			{
				current_info_view.remove_prefix (sizeof ("FS:") - 1);

				feed = atof (current_info_view.data ());

				current_info_view.remove_prefix (
				    current_info_view.find_first_of (',') + 1);

				spindleVal = atoi (current_info_view.data ());
			}
			else
			{
				current_info_view.remove_prefix (sizeof ("F:") - 1);

				feed = atoi (current_info_view.data ());
			}

			parsed_info |= InfoMask::kSpindleAndFeed;
		}
		else if (
		    !(parsed_info & InfoMask::kWco) &&
		    current_info_view.starts_with ("WCO:"))
		{
			current_info_view.remove_prefix (sizeof ("WCO:") - 1);

			auto const wco = ConsumeCoordinatesTriplet (current_info_view);

			ofsX = wco.x;
			ofsY = wco.y;
			ofsZ = wco.z;

			parsed_info |= InfoMask::kWco;
		}
		else if (
		    !(parsed_info & InfoMask::kInputPinState) &&
		    current_info_view.starts_with ("Pn:"))
		{
			static auto constexpr kPnPrefixSize = sizeof ("Pn:") - 1;

			input_pin_state_ = String{
			    current_info_view.data () + kPnPrefixSize,
			    static_cast< unsigned int > (
			        next_separator_pos - kPnPrefixSize + 1)};

			parsed_info |= InfoMask::kInputPinState;
		}
	}

	notify_observers (DeviceStatusEvent{0});
}
