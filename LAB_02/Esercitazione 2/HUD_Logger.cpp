#include "HUD_Logger.h"
#include <GL/freeglut.h>

using namespace std;

HUD_Logger* HUD_Logger::logger;
bool HUD_Logger::ready;
int text_box_height = 100;
typedef struct {
	float r;
	float g;
	float b;
	float a;
} Color;
Color text_color = { 1.0f, 1.0f, 1.0f, 1.0f };

HUD_Logger::HUD_Logger(){}

void HUD_Logger::set_text_box_height(int height) {
	text_box_height = height;
}

void HUD_Logger::set_text_color(float r, float g, float b, float a) {
	text_color.r = r;
	text_color.g = g;
	text_color.b = b;
	text_color.a = a;
}

HUD_Logger* HUD_Logger::get() {
	if (!ready) {
		logger = new HUD_Logger();
		ready = true;
	}
	return logger;
}

void draw_string(string s, int x, int y) {
	glColor3f(
		1.0f,
		text_color.g,
		text_color.b
	);

	glRasterPos2f(x, y);
	for (int i = 0; i < s.length(); i++) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, s[i]);
	}
}

void HUD_Logger::printInfo(std::vector<std::string> lines, float x, float y) {
	const float offset = (float)text_box_height / lines.size();

	for (int i = 0; i < lines.size(); i++) {
		draw_string(lines[i], x, y - (i + 1) * offset);
	}
}