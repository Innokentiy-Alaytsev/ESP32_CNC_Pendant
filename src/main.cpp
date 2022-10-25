#include <Arduino.h>

#include <SD.h>
#include <SPI.h>
#include <U8g2lib.h>

#include "InetServer.h"
#include "Job.h"
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

Display         display;
FileChooser     fileChooser;
ToolTable< 25 > tool_table;
SpindleControl  spindle_control;
uint8_t         droBuffer[ sizeof (GrblDRO) ];
DRO*            dro;
Mode            cMode = Mode::DRO;

void encISR ();
void bt1ISR ();
void bt2ISR ();
void bt3ISR ();

bool detectPrinterAttempt (uint32_t speed, uint8_t type);
void detectPrinter ();

void         deviceLoop (void*);
TaskHandle_t deviceTask;

void         wifiLoop (void*);
TaskHandle_t wifiTask;

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
	attachInterrupt (PIN_BT1, bt1ISR, CHANGE);
	attachInterrupt (PIN_BT2, bt2ISR, CHANGE);
	attachInterrupt (PIN_BT3, bt3ISR, CHANGE);

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
	fileChooser.setCallback ([ & ] (bool res, String path) {
		if (res)
		{
			DEBUGF ("Starting job %s\n", path.c_str ());

			job->setFile (path);

			job->start ();
			job->pause ();

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

void loop ()
{
	readPots ();

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

IRAM_ATTR void btChanged (uint8_t button, uint8_t val)
{
	Display::buttonPressed[ button ] = val == LOW;
}

IRAM_ATTR void bt1ISR ()
{
	static long lastChangeTime = millis ();
	if (millis () < lastChangeTime + 10)
		return;
	lastChangeTime = millis ();
	btChanged (0, digitalRead (PIN_BT1));
}

IRAM_ATTR void bt2ISR ()
{
	static long lastChangeTime = millis ();
	if (millis () < lastChangeTime + 10)
		return;
	lastChangeTime = millis ();
	btChanged (1, digitalRead (PIN_BT2));
}

IRAM_ATTR void bt3ISR ()
{
	static long lastChangeTime = millis ();
	if (millis () < lastChangeTime + 10)
		return;
	lastChangeTime = millis ();
	btChanged (2, digitalRead (PIN_BT3));
}
