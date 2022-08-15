#ifndef SRC_UI_DRO_H
#define SRC_UI_DRO_H


#include "Screen.h"


using JogAxis = int;
using JogDist = int;


class DRO : public Screen {
public:
	DRO ();

	void begin () override;

	void loop () override;

	void enableRefresh (bool r);

	bool isRefreshEnabled ();


protected:
	JogAxis  cAxis;
	JogDist  cDist;
	uint32_t nextRefresh;
	uint32_t lastJogTime;

	bool refresh_enabled_{true};


	static char axisChar (const JogAxis& a);

	static float distVal (const JogDist& a);

	void drawAxis (char axis, float v, int y);

	void drawContents () override;

	void onPotValueChanged (int pot, int v) override;

	void onButtonPressed (Button bt, int8_t arg) override;
};


#endif // SRC_UI_DRO_H
