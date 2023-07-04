#pragma once
#include <string>
#include <vector>

/*
Questa classe statica contiene un singleton utilizzato come ipotetico logger a schermo.
*/
class HUD_Logger {
private:
	HUD_Logger();
public:
	static HUD_Logger* get(); 
	void printInfo(std::vector<std::string> lines, float x, float y);
	void set_text_box_height(int height);
	void set_text_color(float r, float g, float b, float a);
private:
	static HUD_Logger* logger;
	static bool ready;
};

