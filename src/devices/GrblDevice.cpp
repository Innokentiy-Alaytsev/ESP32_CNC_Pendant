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
	if (startsWith (i_resp, "ok"))
	{
		sentQueue.pop ();

		connected = true;
		panic     = false;
	}
	else if (startsWith (i_resp, "error") || startsWith (i_resp, "ALARM:"))
	{
		sentQueue.pop ();

		panic = true;

		GD_DEBUGF ("ERR '%s'\n", resp);

		notify_observers (DeviceStatusEvent{1});

		lastResponse = i_resp;
	}
	else if (startsWith (i_resp, "<"))
	{
		parseGrblStatus (i_resp + 1);
	}
	else if (startsWith (i_resp, "[MSG:"))
	{
		GD_DEBUGF ("Msg '%s'\n", resp);
		lastResponse = i_resp;
	}

	GD_DEBUGF (
	    " > (f%3d,%3d) '%s' \n",
	    sentQueue.getFreeLines (),
	    sentQueue.getFreeBytes (),
	    resp);
}


void mystrcpy (char* dst, const char* start, const char* end)
{
	while (start != end)
	{
		*(dst++) = *(start++);
	}

	*dst = 0;
}


void GrblDevice::parseGrblStatus (char* i_status_string)
{
	//<Idle|MPos:9.800,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
	//<Idle|MPos:9.800,0.000,0.000|FS:0,0|Ov:100,100,100>

	char buf[ 10 ];
	char cpy[ 100 ];

	strncpy (cpy, i_status_string, 100);

	i_status_string = cpy;

	// idle/jogging
	char* pch = strtok (i_status_string, "|");

	if (pch == nullptr)
	{
		return;
	}

	status = pch;

	// MPos:0.000,0.000,0.000
	pch = strtok (nullptr, "|");

	if (pch == nullptr)
	{
		return;
	}

	char *st, *fi;

	st = pch + 5;
	fi = strchr (st, ',');
	mystrcpy (buf, st, fi);
	x = atof (buf);

	st = fi + 1;
	fi = strchr (st, ',');
	mystrcpy (buf, st, fi);
	y = atof (buf);

	st = fi + 1;
	z  = atof (st);

	// FS:500,8000 or F:500
	pch = strtok (nullptr, "|");

	while (pch != nullptr)
	{

		if (startsWith (pch, "FS:") || startsWith (pch, "F:"))
		{
			if (pch[ 1 ] == 'S')
			{
				st = pch + 3;
				fi = strchr (st, ',');

				mystrcpy (buf, st, fi);

				feed = atoi (buf);

				st         = fi + 1;
				spindleVal = atoi (st);
			}
			else
			{
				feed = atoi (pch + 2);
			}
		}
		else if (startsWith (pch, "WCO:"))
		{
			st = pch + 4;
			fi = strchr (st, ',');
			mystrcpy (buf, st, fi);
			ofsX = atof (buf);

			st = fi + 1;
			fi = strchr (st, ',');
			mystrcpy (buf, st, fi);
			ofsY = atof (buf);

			st   = fi + 1;
			ofsZ = atof (st);
		}

		pch = strtok (nullptr, "|");
	}

	notify_observers (DeviceStatusEvent{0});
}
