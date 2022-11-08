#ifndef SRC_DEVICES_GRBLDEVICE_HPP
#define SRC_DEVICES_GRBLDEVICE_HPP


#include <etl/string_view.h>

#include "GCodeDevice.h"


class GrblDevice : public GCodeDevice {
public:
	GrblDevice (Stream* s)
	    : GCodeDevice (s, 20, 100)
	{
		typeStr     = "grbl";
		sentCounter = &sentQueue;
		canTimeout  = false;
	};


	GrblDevice ()
	    : GCodeDevice ()
	{
		typeStr     = "grbl";
		sentCounter = &sentQueue;
	}


	virtual ~GrblDevice () = default;

	const String& InputPinState () const noexcept
	{
		return input_pin_state_;
	}


	bool jog (uint8_t axis, float dist, int feed) override;

	bool canJog () override;


	virtual void begin ()
	{
		GCodeDevice::begin ();

		schedulePriorityCommand ("$I");

		schedulePriorityCommand ("?");
	}


	virtual void reset ()
	{
		panic = false;

		cleanupQueue ();

		char c = 0x18;

		schedulePriorityCommand (&c, 1);
	}


	virtual void requestStatusUpdate () override
	{
		schedulePriorityCommand ("?");
	}


	/// WPos = MPos - WCO
	float getXOfs ()
	{
		return ofsX;
	}


	float getYOfs ()
	{
		return ofsY;
	}


	float getZOfs ()
	{
		return ofsZ;
	}


	uint getSpindleVal ()
	{
		return spindleVal;
	}


	uint getFeed ()
	{
		return feed;
	}


	const String& getStatus ()
	{
		return status;
	}


	const String& getLastResponse ()
	{
		return lastResponse;
	}


protected:
	void trySendCommand () override;

	void tryParseResponse (char* cmd, size_t len) override;


private:
	SimpleCounter< 15, 128 > sentQueue;

	String lastResponse;

	String status;

	String input_pin_state_;

	// WPos = MPos - WCO
	float ofsX, ofsY, ofsZ;
	uint  feed, spindleVal;

	void parseGrblStatus (etl::string_view i_status_string);

	bool isCmdRealtime (char* data, size_t len);
};


#endif // SRC_DEVICES_GRBLDEVICE_HPP
