#include <Arduino.h>

#include <SD.h>
#include <SPI.h>
#include <U8g2lib.h>

#include <etl/string_view.h>

#include "InetServer.h"
#include "Job.h"
#include "WCharacter.h"
#include "devices/GCodeDevice.h"

#include "ui/DRO.h"
#include "ui/FileChooser.h"
#include "ui/GrblDRO.h"
#include "ui/SpindleControl.hpp"
#include "ui/ToolTable.hpp"


HardwareSerial PrinterSerial (2);

#ifdef DEBUGF
#	undef DEBUGF
#endif
#define DEBUGF(...)                                                            \
	{                                                                          \
		Serial.printf (__VA_ARGS__);                                           \
	}
#define DEBUGFI(...)                                                           \
	{                                                                          \
		log_printf (__VA_ARGS__);                                              \
	}
#define DEBUGS(s)                                                              \
	{                                                                          \
		Serial.println (s);                                                    \
	}

#define PIN_POT1 33
#define PIN_POT2 32

#define PIN_BT1 14
#define PIN_BT2 12
#define PIN_BT3 13

#define PIN_ENC1 26
#define PIN_ENC2 27

#define PIN_CE_SD 5
#define PIN_CE_LCD 4
#define PIN_RST_LCD 22

#define PIN_SCL_LCD 2  // E on LCD
#define PIN_SDA_LCD 15 // R/W on LCD


// U8G2_ST7920_128X64_F_HW_SPI u8g2_{U8G2_R3, PIN_CE_LCD, PIN_RST_LCD};

U8G2_ST7920_128X64_F_SW_SPI u8g2_ (
    U8G2_R3,
    /* clock=*/PIN_SCL_LCD,
    /* data=*/PIN_SDA_LCD,
    /* CS=*/PIN_CE_LCD,
    /* reset=*/PIN_RST_LCD);


U8G2& Display::u8g2 = u8g2_;


WebServer server;

GCodeDevice* dev = nullptr;

Job* job;

DynamicJsonDocument grbl_dro_config{512};

enum class Mode { DRO, FILECHOOSER };

using GrblToolTable = ToolTable< 25 >;

Display        display;
FileChooser    fileChooser;
GrblToolTable  tool_table;
SpindleControl spindle_control;
uint8_t        droBuffer[ sizeof (GrblDRO) ];
DRO*           dro;
Mode           cMode = Mode::DRO;

void encISR ();

template < size_t KButtonIndex, uint8_t KPin >
void ButtonISR ();

bool detectPrinterAttempt (uint32_t speed, uint8_t type);
void detectPrinter ();

void         deviceLoop (void*);
TaskHandle_t deviceTask;

void         wifiLoop (void*);
TaskHandle_t wifiTask;


int ParseToolNumberFromFileName (const String& i_file_name)
{
	static auto constexpr kSeparator = '.';


	auto file_name =
	    etl::string_view{i_file_name.c_str (), i_file_name.length ()};

	if (auto const slash_position = file_name.find_last_of ('/');
	    etl::string_view::npos != slash_position)
	{
		file_name.remove_prefix (slash_position + 1);
	}

	for (auto next_separator_pos = file_name.find_first_of (kSeparator);
	     etl::string_view::npos != next_separator_pos;
	     file_name.remove_prefix (next_separator_pos + 1),
	          next_separator_pos = file_name.find_first_of (kSeparator))
	{
		auto substr = etl::string_view{file_name.data (), next_separator_pos};

		if (!substr.starts_with ('T') || (1 >= substr.size ()) ||
		    (substr.size () > (GrblToolTable::kToolNumberMaxDigits + 1)))
		{
			continue;
		}

		substr.remove_prefix (substr.find_first_not_of ('0', 1));

		if (0 == substr.size ())
		{
			continue;
		}

		auto const substr_tool_number = atoi (substr.data ());

		if (0 >= substr_tool_number)
		{
			continue;
		}

		return substr_tool_number;
	}

	return GrblToolTable::kNoToolId;
}


void setup ()
{
	Serial.begin (115200);

	pinMode (PIN_BT1, INPUT_PULLUP);
	pinMode (PIN_BT2, INPUT_PULLUP);
	pinMode (PIN_BT3, INPUT_PULLUP);

	pinMode (PIN_ENC1, INPUT_PULLUP);
	pinMode (PIN_ENC2, INPUT_PULLUP);

	attachInterrupt (PIN_ENC1, encISR, CHANGE);
	attachInterrupt (PIN_ENC2, encISR, CHANGE);
	attachInterrupt (PIN_BT1, ButtonISR< 0, PIN_BT1 >, FALLING);
	attachInterrupt (PIN_BT2, ButtonISR< 1, PIN_BT2 >, FALLING);
	attachInterrupt (PIN_BT3, ButtonISR< 2, PIN_BT3 >, FALLING);

	u8g2_.begin ();
	u8g2_.setBusClock (600000);
	u8g2_.setFont (u8g2_font_5x8_tr);
	u8g2_.setFontPosTop ();
	u8g2_.setFontMode (1);

	digitalWrite (PIN_RST_LCD, LOW);
	delay (100);
	digitalWrite (PIN_RST_LCD, HIGH);

	Serial.print ("Initializing SD card...");

	if (!SD.begin (PIN_CE_SD))
	{
		Serial.println ("initialization failed!");
		while (1)
			;
	}
	Serial.println ("initialization done.");

	DynamicJsonDocument  cfg (4096);
	File                 file  = SD.open ("/config.json");
	DeserializationError error = deserializeJson (cfg, file);
	if (error)
		Serial.println (F ("Failed to read file, using default configuration"));

	server.config (cfg[ "web" ].as< JsonObjectConst > ());
	server.add_observer (display);

	if (auto const grbl_dro_conf_doc = cfg[ "GRBL DRO" ];
	    !grbl_dro_conf_doc.isNull ())
	{
		grbl_dro_config.set (grbl_dro_conf_doc);
	}

	xTaskCreatePinnedToCore (
	    deviceLoop,
	    "DeviceTask",
	    4096,
	    nullptr,
	    1,
	    &deviceTask,
	    1); // cpu1

	xTaskCreatePinnedToCore (
	    wifiLoop,
	    "WifiTask",
	    4096,
	    nullptr,
	    1,
	    &wifiTask,
	    1); // cpu1

	job = Job::getJob ();
	job->add_observer (display);

	// dro.config(cfg["menu"].as<JsonObjectConst>() );

	tool_table.ApplyConfig (
	    cfg[ "tool_compensation" ].as< JsonObjectConst > ());
	tool_table.SetReturnCallback (
	    [ &dro ] () { Display::getDisplay ()->setScreen (dro); });

	spindle_control.ApplyConfig (
	    cfg[ "spindle_control" ].as< JsonObjectConst > ());
	spindle_control.SetReturnCallback (
	    [ &dro ] () { Display::getDisplay ()->setScreen (dro); });

	fileChooser.begin ();
	fileChooser.setCallback ([ & ] (bool res, const String& path) {
		if (res)
		{
			DEBUGF ("Starting job %s\n", path.c_str ());

			if (dev->getType () == "grbl")
			{
				if (auto const file_tool = ParseToolNumberFromFileName (path);
				    GrblToolTable::kNoToolId != file_tool)
				{
					tool_table.SetActiveTool (file_tool, 'A');
				}
			}

			job->setFile (path);
			job->start ();

			Display::getDisplay ()->setScreen (dro); // select file
		}
		else
		{
			Display::getDisplay ()->setScreen (dro); // cancel
		}
	});

	file.close ();
}

void deviceLoop (void* pvParams)
{
	PrinterSerial.begin (115200);
	PrinterSerial.setTimeout (1000);
	dev = DeviceDetector::detectPrinterAttempt (PrinterSerial, 115200, 1);
	if (dev == nullptr)
	{
		dev = DeviceDetector::detectPrinter (PrinterSerial);
	}

	// GCodeDevice::setDevice(dev);
	dev->add_observer (*job);
	dev->add_observer (display);
	// dev->add_observer(dro);  // dro.setDevice(dev);
	// dev->add_observer(fileChooser);
	dev->addReceivedLineHandler (
	    [] (const char* d, size_t l) { server.resendDeviceResponse (d, l); });
	dev->begin ();

	if (dev->getType () == "grbl")
	{
		auto const grbl_dro = new (droBuffer) GrblDRO ();

		grbl_dro->ApplyConfig (grbl_dro_config.as< JsonObjectConst > ());

		dev->add_observer (*grbl_dro);

		dro = grbl_dro;

		dev->add_observer (spindle_control);
	}
	else
		dro = new (droBuffer) DRO ();

	dro->begin ();

	display.setScreen (dro);

	while (1)
	{
		dev->loop ();
	}
	vTaskDelete (NULL);
}

void wifiLoop (void* args)
{
	server.begin ();
	vTaskDelete (NULL);
}

void readPots ()
{
	Display::potVal[ 0 ] = analogRead (PIN_POT1);
	Display::potVal[ 1 ] = analogRead (PIN_POT2);
}


struct ButtonInput {
	uint8_t write_state : 1;
	uint8_t read_state : 1;

	bool state[ 2 ]{};

	uint32_t last_change_time{};
};


static constexpr uint32_t kDebouncePeriod = 250;


ButtonInput buttons[ 3 ]{{0, 1}, {0, 1}, {0, 1}};


void loop ()
{
	readPots ();

	const auto current_time = millis ();

	for (auto&& button : buttons)
	{
		if (kDebouncePeriod > (current_time - button.last_change_time))
		{
			continue;
		}

		Display::buttonPressed[ &button - &buttons[ 0 ] ] =
		    button.state[ button.read_state ];

		button.state[ button.read_state ] = false;

		button.read_state++;
		button.write_state++;
	}

	job->loop ();

	display.loop ();

	if (dev == nullptr)
		return;

	static String s;
	while (Serial.available () != 0)
	{
		char c = Serial.read ();
		if (c == '\n' || c == '\r')
		{
			if (s.length () > 0)
				DEBUGF (
				    "send %s, result: %d\n",
				    s.c_str (),
				    dev->schedulePriorityCommand (s));
			s = "";
			continue;
		}
		s += c;
	}

	delay (10);
}


IRAM_ATTR void encISR ()
{
	static constexpr int8_t gray_code_to_increment[] = {
	    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

	static uint8_t gray_code{static_cast< uint8_t > (
	    digitalRead (PIN_ENC2) << 1 | digitalRead (PIN_ENC1))};

	static int64_t counter{0};

	gray_code <<= 2;
	gray_code |= static_cast< uint8_t > (
	    (digitalRead (PIN_ENC2) << 1) | digitalRead (PIN_ENC1));

	counter += gray_code_to_increment[ gray_code & 0xF ];

	Display::encVal = (counter >> 2);
}


template < size_t KButtonIndex, uint8_t KPin >
IRAM_ATTR void ButtonISR ()
{
	auto& button = buttons[ KButtonIndex ];

	button.state[ button.write_state ] = (LOW == digitalRead (KPin));
	button.last_change_time            = millis ();
}
