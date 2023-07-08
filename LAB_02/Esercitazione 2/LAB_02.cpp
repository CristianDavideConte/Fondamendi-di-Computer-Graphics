#include <iostream>
#include "ShaderMaker.h"
#include <GL/glew.h>
#include <GL/freeglut.h>

static unsigned int programId;
unsigned int MatProj, MatModel;

// Include GLM; libreria matematica per le opengl
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
using namespace glm;

#include <chrono>
#include "ColorPalettes.h"
#include <map>
#include <cmath>
#include <time.h>
#include "HUD_Logger.h"

#define __DEBUG__ true

#define PI 3.14159265358979323846f

#define TRACK_HORIZON_HEIGHT ((float)window_height * 1.5f)

#define HIGHWAY_LINES_WIDTH 20.0f
#define STATIC_HIGHWAY_LINES_NUM 4
#define HIGHWAY_LINES_POINTS_NUM 2
#define MOVING_HIGHWAY_LINES_NUM 3
#define MOVING_HIGHWAY_LINES_SPEED (3.35 * game_speed)
#define MOVING_HIGHWAY_LINES_SPACE 50

#define OBSTACLE_ON_SCREEN_MAX_NUM 7
#define OBSTACLE_TRIANGLES_NUM 3 //4
#define OBSTACLE_SIZE ((float) window_height/ 18.0f)

#define CIRCLE_TRIANGLES_NUM 20
#define BALL_MIN_LINE_HEIGHT ((float)window_height * 0.4f)
#define BALL_MAX_LINE_HEIGHT ((float)window_height * 0.7f)
#define BALL_X_MOVEMENT_MAX_DURATION (2.0f * 1000.0f)
#define BALL_X_MOVEMENT_MIN_DURATION (1.0f * 1000.0f)
#define BALL_Y_MOVEMENT_MAX_DURATION (3.0f * 1000.0f)
#define BALL_Y_MOVEMENT_MIN_DURATION (1.0f * 1000.0f)

#define GAME_MAX_DIFFICULTY_IN_S 40 //The game max difficulty is reached after this number of seconds
#define GAME_MAX_DURATION_THRESHOLD (GAME_MAX_DIFFICULTY_IN_S * 1000) //in s
#define GAME_INITIAL_ACC 0.3f

#define LEFT_MOVE_KEY 'a'
#define RIGHT_MOVE_KEY 'd'
#define JUMP_MOVE_KEY ' '


typedef struct { float x, y, r, g, b, a; } PointRGBA;
typedef struct { float r, g, b, a; } Color;
typedef struct { PointRGBA points[HIGHWAY_LINES_POINTS_NUM]; } Line;
typedef struct {
	float width;
	float height;
	PointRGBA points[CIRCLE_TRIANGLES_NUM * 3];
} Circle;
typedef struct {
	int moving_line_index;

	float initial_x;
	float initial_y;
	float current_x;
	float current_y;
	float final_x;
	float final_y;

	long long movement_x_timestamp; //the timestamp at which the x-movement started
	long long movement_y_timestamp; //the timestamp at which the y-movement started

	bool is_jumping;
	Circle circle;
	Circle shadow;
} Ball;
typedef struct {
	PointRGBA points[OBSTACLE_TRIANGLES_NUM * 3 + 1];
	int moving_line_index;
} Obstacle;

Line horizon_line{};

Line static_highway_lines[STATIC_HIGHWAY_LINES_NUM]{};
unsigned int static_highway_lines_VAOs[STATIC_HIGHWAY_LINES_NUM]{};
unsigned int static_highway_lines_VBOs[STATIC_HIGHWAY_LINES_NUM]{};

Line moving_highway_lines[MOVING_HIGHWAY_LINES_NUM]{};
unsigned int moving_highway_lines_VAOs[MOVING_HIGHWAY_LINES_NUM]{};
unsigned int moving_highway_lines_VBOs[MOVING_HIGHWAY_LINES_NUM]{};

PointRGBA track_vertices[3]{};
unsigned int track_VAO{};
unsigned int track_VBO{};
Color track_color; 

unsigned int ball_VAO{};
unsigned int ball_VBO{};
unsigned int ball_shadow_VAO{};
unsigned int ball_shadow_VBO{};
Color ball_top_color = { BALL_COLOR_TOP };
Color ball_bottom_color = { BALL_COLOR_BOTTOM };
Color ball_shadow_top_color = { BALL_SHADOW_COLOR_TOP };
Color ball_shadow_bottom_color = { BALL_SHADOW_COLOR_BOTTOM };

Ball ball{};

unsigned int overlay_VAO{};
unsigned int overlay_VBO{};

map<unsigned char, bool> is_key_pressed{
	{ LEFT_MOVE_KEY, false },
	{ RIGHT_MOVE_KEY, false },
	{ JUMP_MOVE_KEY, false }
};

// viewport size
int window_width = 1200;
int window_height = 720;

int current_obstacle_min_index = 0;
int current_obstacle_max_index = -1;

float game_speed = 1.0f; 
float game_speed_acc = GAME_INITIAL_ACC;

float game_start_time = -1;
float obstacle_track_line_spawn_weights[3] = { 0.33f, 0.34f, 0.33f };

long long game_score = 0.0f;
bool game_over = false;

mat4 Projection;  //Matrice di proiezione
mat4 Model; //Matrice per il cambiamento da Sistema di riferimento dell'oggetto OCS a sistema di riferimento nel Mondo WCS

bool check_collision(Obstacle* obs, Ball* ball);
bool point_is_in_triangle(PointRGBA p, PointRGBA v1, PointRGBA v2, PointRGBA v3);


//return a random number in [0..1]
float get_rand() {
	return static_cast <float>(rand()) / static_cast <float> (RAND_MAX);
}

float lerp(float a, float b, float amount) {
	return (1 - amount) * a + amount * b;
}

float get_x_of_line_between_P1_P2(float y, PointRGBA p1, PointRGBA p2) {
	return p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
}

void copy_P1_into_P2(PointRGBA* p1, PointRGBA* p2) {
	p2->x = p1->x;
	p2->y = p1->y;
	p2->r = p1->r;
	p2->g = p1->g;
	p2->b = p1->b;
	p2->a = p1->a;
}

long long get_current_time() {
	return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}

void update_game_speed() {
	if (game_over) return;

	const auto now = get_current_time();
	
	if (game_start_time < 0) {
		game_start_time = now;
	}
	
	const auto duration = now - game_start_time;
	const auto max_duration = GAME_MAX_DURATION_THRESHOLD;

	//Ease-out-sine pattern
	const auto easing_x = std::min(1.0f, duration / max_duration); //in [0..1]
	const auto easing_y = std::sin((easing_x * PI) / 2); //in [0..1]
    
	game_speed_acc = std::max(game_speed_acc, easing_y);
	game_speed += (4.5f * game_speed_acc);

	//Avoid a float overflow
	if (game_speed > 1000) {
		game_speed /= 9.5;
	}

	//update the game score
	game_score += (10 * game_speed_acc);
}






void set_track_color(Color* track_color) {
	Color track_color_dark = { TRACK_COLOR_DARK };
	Color track_color_light = { TRACK_COLOR_LIGHT };

	track_color->r = lerp(track_color_dark.r, track_color_light.r, game_speed_acc);
	track_color->g = lerp(track_color_dark.g, track_color_light.g, game_speed_acc);
	track_color->b = lerp(track_color_dark.b, track_color_light.b, game_speed_acc);
	track_color->a = 1.0f;
}

void draw_highway() {
	PointRGBA static_p0 = { 0.0f, 0, TRACK_LINES_COLOR };
	PointRGBA static_p1_left = { (float)window_width / 3.0f , 0, TRACK_LINES_COLOR };
	PointRGBA static_p1_right = { (float)window_width / 3.0f * 2.0f, 0, TRACK_LINES_COLOR };
	PointRGBA static_p2 = { (float)window_width / 2.0f, TRACK_HORIZON_HEIGHT, TRACK_LINES_COLOR };
	PointRGBA static_p3 = { (float)window_width, 0, TRACK_LINES_COLOR };

	PointRGBA moving_p1_left = { (float)window_width / 3.0f / 2.0f, 0, TRACK_LINES_COLOR };
	PointRGBA moving_p1_right = { (float)window_width / 3.0f * 2.0f + (float)window_width / 3.0f / 2.0f, 0, TRACK_LINES_COLOR };
	PointRGBA moving_p2 = { (float)window_width / 2.0f, 0, TRACK_LINES_COLOR };

	/* Static lines init */

	//Init left-track's left line
	copy_P1_into_P2(&static_p0, &static_highway_lines[0].points[0]);
	copy_P1_into_P2(&static_p0, &static_highway_lines[0].points[1]);
	static_highway_lines[0].points[1].x = static_p2.x; 
	static_highway_lines[0].points[1].y = static_p2.y; 


	//Init center-track's left line
	copy_P1_into_P2(&static_p1_left, &static_highway_lines[1].points[0]);
	copy_P1_into_P2(&static_p1_left, &static_highway_lines[1].points[1]);
	static_highway_lines[1].points[1].x = static_p2.x; 
	static_highway_lines[1].points[1].y = static_p2.y; 


	//Init center-track's right line
	copy_P1_into_P2(&static_p1_right, &static_highway_lines[2].points[0]);
	copy_P1_into_P2(&static_p1_right, &static_highway_lines[2].points[1]);
	static_highway_lines[2].points[1].x = static_p2.x; 
	static_highway_lines[2].points[1].y = static_p2.y; 


	//Init right-track's right line
	copy_P1_into_P2(&static_p3, &static_highway_lines[3].points[0]);
	copy_P1_into_P2(&static_p3, &static_highway_lines[3].points[1]);
	static_highway_lines[3].points[1].x = static_p2.x;
	static_highway_lines[3].points[1].y = static_p2.y; 


	//Moving lines init
	//Leave the top point fixed, so that the line 
	//looks like it's moving (it's actually stretching)

	//Init left-track's center (moving) line
	copy_P1_into_P2(&moving_p1_left, &moving_highway_lines[0].points[0]);
	moving_highway_lines[0].points[0].x = get_x_of_line_between_P1_P2(-MOVING_HIGHWAY_LINES_SPEED, moving_p1_left, static_p2);
	moving_highway_lines[0].points[0].y = -MOVING_HIGHWAY_LINES_SPEED;
	copy_P1_into_P2(&static_p2, &moving_highway_lines[0].points[1]);

	//Init center-track's center (moving) line
	copy_P1_into_P2(&moving_p2, &moving_highway_lines[1].points[0]);
	moving_highway_lines[1].points[0].x = get_x_of_line_between_P1_P2(-MOVING_HIGHWAY_LINES_SPEED, moving_p2, static_p2);
	moving_highway_lines[1].points[0].y = -MOVING_HIGHWAY_LINES_SPEED;
	copy_P1_into_P2(&static_p2, &moving_highway_lines[1].points[1]);

	//Init right-track's center (moving) line
	copy_P1_into_P2(&moving_p1_right, &moving_highway_lines[2].points[0]);
	moving_highway_lines[2].points[0].x = get_x_of_line_between_P1_P2(-MOVING_HIGHWAY_LINES_SPEED, moving_p1_right, static_p2);
	moving_highway_lines[2].points[0].y = -MOVING_HIGHWAY_LINES_SPEED;
	copy_P1_into_P2(&static_p2, &moving_highway_lines[2].points[1]);


	//Init the center track colors
	copy_P1_into_P2(&static_p0, &track_vertices[0]);
	copy_P1_into_P2(&static_p2, &track_vertices[1]);
	copy_P1_into_P2(&static_p3, &track_vertices[2]);


	Color track_color_dark = { TRACK_COLOR_DARK };
	Color track_color{};
	set_track_color(&track_color);

	//bottom-left vertex
	track_vertices[0].r = track_color.r;
	track_vertices[0].g = track_color.g;
	track_vertices[0].b = track_color.b;
	track_vertices[0].a = track_color.a;

	//top vertex
	track_vertices[1].r = track_color_dark.r;
	track_vertices[1].g = track_color_dark.g;
	track_vertices[1].b = track_color_dark.b;
	track_vertices[1].a = track_color_dark.a;

	//bottom-right vertex
	track_vertices[2].r = track_color.r;
	track_vertices[2].g = track_color.g;
	track_vertices[2].b = track_color.b;
	track_vertices[2].a = track_color.a;

	//Reset the projection matrix
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(mat4(1.0)));

	//Draw the highway's track 
	glBindVertexArray(track_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, track_VBO);
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(PointRGBA), &track_vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glPolygonMode(GL_FRONT, GL_FILL);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	//Draw the static highway lines
	for (int i = 0; i < STATIC_HIGHWAY_LINES_NUM; i++) {
		glBindVertexArray(static_highway_lines_VAOs[i]);
		glBindBuffer(GL_ARRAY_BUFFER, static_highway_lines_VBOs[i]);
		glBufferData(GL_ARRAY_BUFFER, HIGHWAY_LINES_POINTS_NUM * sizeof(PointRGBA), &static_highway_lines[i].points[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glLineWidth(HIGHWAY_LINES_WIDTH);
		glDrawArrays(GL_LINES, 0, HIGHWAY_LINES_POINTS_NUM);
		glBindVertexArray(0);
	}

	//Sets the lines' style to dashed-line
	glPushAttrib(GL_ENABLE_BIT);
	glLineStipple(MOVING_HIGHWAY_LINES_SPACE, 0xAAAA);
	glEnable(GL_LINE_STIPPLE);

	//Draw the moving highway lines
	for (int i = 0; i < MOVING_HIGHWAY_LINES_NUM; i++) {
		glBindVertexArray(moving_highway_lines_VAOs[i]);
		glBindBuffer(GL_ARRAY_BUFFER, moving_highway_lines_VBOs[i]);
		glBufferData(GL_ARRAY_BUFFER, HIGHWAY_LINES_POINTS_NUM * sizeof(PointRGBA), &moving_highway_lines[i].points[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glLineWidth(HIGHWAY_LINES_WIDTH);
		glDrawArrays(GL_LINES, 0, HIGHWAY_LINES_POINTS_NUM);
		glBindVertexArray(0);
	}

	//Reset the lines' style to 
	glDisable(GL_LINE_STIPPLE);
	glPopAttrib();
}













Obstacle obstacle_list[OBSTACLE_ON_SCREEN_MAX_NUM]{};
unsigned int obstacles_VAOs[OBSTACLE_ON_SCREEN_MAX_NUM]{};
unsigned int obstacles_VBOs[OBSTACLE_ON_SCREEN_MAX_NUM]{};

void swap_obstacles(Obstacle* obs1, Obstacle* obs2) {
	Obstacle* tmp = obs1;
	*obs1 = *obs2;
	*obs2 = *tmp;
}

//Chooses a track line number in a pseudo-uniform manner
//rather than completly random
int get_obstacle_track_line() {
	const float rnd = get_rand();
	float current_probability = 0.0f;

	for (int i = 0; i < MOVING_HIGHWAY_LINES_NUM; i++) {
		current_probability += obstacle_track_line_spawn_weights[i];
		if (rnd <= current_probability) {

			const float probability_loss = obstacle_track_line_spawn_weights[i];
			const float probability_gains = probability_loss / 2.0f;

			obstacle_track_line_spawn_weights[i] -= probability_loss;

			//Redistribute the weight lost by a line to the others
			for (int j = 0; j < MOVING_HIGHWAY_LINES_NUM; j++) {
				if (i == j) continue;
				obstacle_track_line_spawn_weights[j] += probability_gains;
			}
			 
			return i;
		}
	}

	//Too many float approximation mistakes, go back to an even distribution
	obstacle_track_line_spawn_weights[0] = 0.33f;
	obstacle_track_line_spawn_weights[1] = 0.34f;
	obstacle_track_line_spawn_weights[2] = 0.33f;
	return 1;
}

void generate_obstacle_points(
	PointRGBA central_point,
	PointRGBA left_line_p1,
	PointRGBA left_line_p2,
	PointRGBA right_line_p1,
	PointRGBA right_line_p2,
	PointRGBA * obstacle_points
) {
	PointRGBA p0 = { get_x_of_line_between_P1_P2(central_point.y - OBSTACLE_SIZE, left_line_p1, left_line_p2), central_point.y - OBSTACLE_SIZE, OBSTACLE_COLOR_BOTTOM };
	PointRGBA p1 = { get_x_of_line_between_P1_P2(central_point.y - OBSTACLE_SIZE, right_line_p1, right_line_p2), central_point.y - OBSTACLE_SIZE, OBSTACLE_COLOR_BOTTOM };
	PointRGBA p2 = { get_x_of_line_between_P1_P2(central_point.y + OBSTACLE_SIZE, right_line_p1, right_line_p2), central_point.y + OBSTACLE_SIZE, OBSTACLE_COLOR_RIGHT };
	PointRGBA p3 = { get_x_of_line_between_P1_P2(central_point.y + OBSTACLE_SIZE, left_line_p1, left_line_p2), central_point.y + OBSTACLE_SIZE, OBSTACLE_COLOR_LEFT };
	PointRGBA p4 = { central_point.x, central_point.y + 0.6 * (p1.x - p0.x), OBSTACLE_COLOR_TOP };


	//Arange the points for a pyramid-like structure
	const float correction_delta_x_front = p1.x - p0.x;
	const float correction_delta_x_back  = p2.x - p3.x;

	p0.x = (p0.x + p1.x) / 2.0f;
	p1.x = p0.x;
	//p0.x += correction_delta_x_front / 8;
	//p1.x -= correction_delta_x_front / 8;
	p2.x -= correction_delta_x_back / 9;
	p3.x += correction_delta_x_back / 9;


	obstacle_points[0] = p0;
	obstacle_points[1] = p1;
	obstacle_points[2] = p2;
	obstacle_points[3] = p3;
	obstacle_points[4] = p4;
	obstacle_points[5] = central_point;
}

void generate_obstacle_triangles(PointRGBA * obstacle_points, Obstacle * obs) {
	//back facing triangle
	copy_P1_into_P2(&obstacle_points[2], &obs->points[0]);
	copy_P1_into_P2(&obstacle_points[4], &obs->points[1]);
	copy_P1_into_P2(&obstacle_points[3], &obs->points[2]);

	//left facing triangle
	copy_P1_into_P2(&obstacle_points[3], &obs->points[3]);
	copy_P1_into_P2(&obstacle_points[4], &obs->points[4]);
	copy_P1_into_P2(&obstacle_points[0], &obs->points[5]);

	//right facing triangle
	copy_P1_into_P2(&obstacle_points[1], &obs->points[6]);
	copy_P1_into_P2(&obstacle_points[4], &obs->points[7]);
	copy_P1_into_P2(&obstacle_points[2], &obs->points[8]);

	//central point
	copy_P1_into_P2(&obstacle_points[5], &obs->points[9]);
}

void generate_obstacle(int min_line_index, int max_line_index, float central_point_y, Obstacle * obs) {
	PointRGBA left_line_p1 = static_highway_lines[min_line_index].points[0];
	PointRGBA left_line_p2 = static_highway_lines[min_line_index].points[1];

	PointRGBA right_line_p1 = static_highway_lines[max_line_index].points[0];
	PointRGBA right_line_p2 = static_highway_lines[max_line_index].points[1];

	PointRGBA central_point = { 
		get_x_of_line_between_P1_P2(
			central_point_y,
			moving_highway_lines[min_line_index].points[0], 
			moving_highway_lines[min_line_index].points[1]
		),
		central_point_y,
		OBSTACLE_COLOR_TOP
	};

	PointRGBA obstacle_points[6]{};
	generate_obstacle_points(
		central_point,
		left_line_p1,
		left_line_p2,
		right_line_p1,
		right_line_p2,
		obstacle_points
	);

	generate_obstacle_triangles(obstacle_points, obs);
	obs->moving_line_index = min_line_index;

	//fade the obstacle away if it's past the ball
	if (obs->points[0].y <= BALL_MIN_LINE_HEIGHT) {
		for (int i = 0; i < 10; i++) {
			obs->points[i].a = obs->points[0].y / BALL_MIN_LINE_HEIGHT;
		}
	}
}

void update_obstacles() {
	//Generate a new obstacle every N seconds
	if (
		(int)(game_speed) % (int)lerp(300, 100, game_speed_acc) <= lerp(1, 4, game_speed_acc) &&
		(current_obstacle_max_index < OBSTACLE_ON_SCREEN_MAX_NUM || current_obstacle_min_index > 0)
	){
		int min_line_index = get_obstacle_track_line();
		int max_line_index = min_line_index + 1;

		if (current_obstacle_min_index > 0) { //Fill the empty space
			current_obstacle_min_index--;
			generate_obstacle(min_line_index, max_line_index, TRACK_HORIZON_HEIGHT, &obstacle_list[current_obstacle_min_index]);
		} else { //Add a new obstacle at the end of the list
			current_obstacle_max_index++;
			generate_obstacle(min_line_index, max_line_index, TRACK_HORIZON_HEIGHT, &obstacle_list[current_obstacle_max_index]);
		}
	}

	//Update the position of each obstacle
	for (int i = current_obstacle_min_index; i < current_obstacle_max_index; i++) {
		const int min_line_index = obstacle_list[i].moving_line_index;
		const int max_line_index = min_line_index + 1;
		const float next_y = game_over ? obstacle_list[i].points[9].y : obstacle_list[i].points[9].y - (15.0f * game_speed_acc);

		generate_obstacle(
			min_line_index, 
			max_line_index, 
			next_y,
			&obstacle_list[i]
		);
		
		//If an obstacle goes out of screen, remove it and increase the score
		if (obstacle_list[i].points[0].a <= 0) {
			//Make the out-of-screen obstacle the first of the list,
			//so that it will not be considered by the indexing
			if(i != current_obstacle_min_index) {
				swap_obstacles(&obstacle_list[i], &obstacle_list[current_obstacle_min_index]);
			}

			current_obstacle_min_index++;
			continue;
		}

		if (check_collision(&obstacle_list[i], &ball)) {
			game_over = true;
		}

		//Draw the obstacle
		glBindVertexArray(obstacles_VAOs[i]);
		glBindBuffer(GL_ARRAY_BUFFER, obstacles_VBOs[i]);
		glBufferData(GL_ARRAY_BUFFER, OBSTACLE_TRIANGLES_NUM * 2 * sizeof(PointRGBA), &obstacle_list[i].points[3], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glDrawArrays(GL_TRIANGLES, 0, OBSTACLE_TRIANGLES_NUM * 3);
		glBindVertexArray(0);
	}
}







void create_circle(Circle * circle, Color color_top, Color color_bottom) {
	const float stepA = (2 * PI) / CIRCLE_TRIANGLES_NUM;
	int vertex_num = 0;

	for (int i = 0; i < CIRCLE_TRIANGLES_NUM; i++) {
		circle->points[vertex_num].x = cos((double)i * stepA);
		circle->points[vertex_num].y = sin((double)i * stepA);
		circle->points[vertex_num].r = color_top.r;
		circle->points[vertex_num].g = color_top.g;
		circle->points[vertex_num].b = color_top.b;
		circle->points[vertex_num].a = color_top.a;

		circle->points[vertex_num + 1].x = cos((double)(i + 1) * stepA);
		circle->points[vertex_num + 1].y = sin((double)(i + 1) * stepA);
		circle->points[vertex_num + 1].r = color_top.r;
		circle->points[vertex_num + 1].g = color_top.g;
		circle->points[vertex_num + 1].b = color_top.b;
		circle->points[vertex_num + 1].a = color_top.a;

		circle->points[vertex_num + 2].x = 0.0;
		circle->points[vertex_num + 2].y = 0.0;
		circle->points[vertex_num + 2].r = color_bottom.r;
		circle->points[vertex_num + 2].g = color_bottom.g;
		circle->points[vertex_num + 2].b = color_bottom.b;
		circle->points[vertex_num + 2].a = color_bottom.a;

		vertex_num += 3;
	}
}

void draw_circle(
	float current_x,
	float current_y,
	Circle circle,
	unsigned int vao,
	unsigned int vbo
) {
	//Matrice per il cambiamento del sistema di rifermento
	Model = mat4(1.0);
	Model = translate(Model, vec3(current_x, current_y, 0.0f));
	Model = scale(Model, vec3(circle.width / 2.0f, circle.height / 2.0f, 1.0));
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Model));

	//Draw the circle
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, CIRCLE_TRIANGLES_NUM * 3 * sizeof(PointRGBA), &circle.points[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDrawArrays(GL_TRIANGLES, 0, CIRCLE_TRIANGLES_NUM * 3);
	glBindVertexArray(0);

	//reset the model matrix to identity
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(mat4(1.0)));
}

void set_ball_shadow_opacity(Circle * shadow, float top_opacity, float bottom_opacity) {
	Color top_color = ball_shadow_top_color;
	Color bottom_color = ball_shadow_bottom_color;
	top_color.a = top_opacity;
	bottom_color.a = bottom_opacity;

	create_circle(shadow, top_color, bottom_color);
}


void update_ball_position() {
	if (game_over) return;

	if (ball.current_x != ball.final_x || ball.movement_x_timestamp > 0) {
		if (ball.movement_x_timestamp < 0) {
			ball.movement_x_timestamp = get_current_time();
		}

		const auto c1 = 1.70158;
		const auto c3 = c1 + 1;
		const auto now = get_current_time();
		const auto duration = now - ball.movement_x_timestamp;
		const auto progress = duration / lerp(BALL_X_MOVEMENT_MAX_DURATION, BALL_X_MOVEMENT_MIN_DURATION, game_speed_acc);
		const auto x = 
			progress <= 0 ? 0 :
			progress >= 1 ? 1 :
			1 + c3 * pow(progress - 1, 3) + c1 * pow(progress - 1, 2); //ease-out-back pattern

		ball.current_x = lerp(
			ball.initial_x,
			ball.final_x,
			x
		);

		if (progress >= 1) {
			ball.current_x = ball.final_x;
			ball.movement_x_timestamp = -1;
		}
	}

	if (ball.current_y != ball.final_y || ball.movement_y_timestamp > 0) {
		if (ball.movement_y_timestamp < 0) {
			ball.movement_y_timestamp = get_current_time();
		}

		const auto now = get_current_time();
		const auto duration = now - ball.movement_y_timestamp;
		auto progress = duration / lerp(BALL_Y_MOVEMENT_MAX_DURATION, BALL_Y_MOVEMENT_MIN_DURATION, game_speed_acc);
		
		if (progress <= 0) {
			ball.current_y = ball.initial_y;
			ball.shadow.width = ball.circle.width;
			ball.shadow.height = ball.circle.height;
			set_ball_shadow_opacity(&ball.shadow, ball_shadow_top_color.a, ball_shadow_bottom_color.a);
			return;
		} 
		
		float pattern{};

		if (progress <= 0.3) {
			progress = (1 - 0) / (0.3 - 0) * progress;
			pattern = sqrt(1 - pow(1 - progress, 2.5)); //ease-out-quad pattern

			ball.current_y = lerp(
				ball.initial_y,
				ball.final_y,
				pattern
			);

			ball.shadow.width = lerp(
				ball.circle.width,
				2 * ball.circle.width,
				pattern
			);
			ball.shadow.height = ball.shadow.width;

			set_ball_shadow_opacity(
				&ball.shadow, 
				lerp(ball_shadow_top_color.a, 0, pattern),
				lerp(ball_shadow_bottom_color.a, ball_shadow_bottom_color.a / 1.7f, pattern)
			);
			return;
		}

		if (progress >= 0.5) {
			ball.is_jumping = false;
		}

		const auto n1 = 7.5625;
		const auto d1 = 2.75;
		progress = (1 - 0) / (0.7 - 0) * progress - 0.42857142;
		
		//elastic bouncing equations
		if (progress < 1 / d1) {
			pattern = n1* progress* progress;
		} else if (progress < 2 / d1) {
			pattern = n1 * (progress - (1.5 / d1)) * (progress - (1.5 / d1)) + 0.75;
		} else if (progress < 2.5 / d1) {
			pattern = n1 * (progress - (2.25 / d1)) * (progress - (2.25 / d1)) + 0.9375;
		} else {
			pattern = n1 * (progress - (2.625 / d1)) * (progress - (2.625 / d1)) + 0.984375;
		}
		
		ball.initial_y = BALL_MAX_LINE_HEIGHT;
		ball.final_y = BALL_MIN_LINE_HEIGHT;

		if (progress >= 1) {
			ball.current_y = ball.final_y;
			ball.movement_y_timestamp = -1;
			ball.shadow.width = ball.circle.width;
			ball.shadow.height = ball.circle.height;
			set_ball_shadow_opacity(&ball.shadow, ball_shadow_top_color.a, ball_shadow_bottom_color.a);
			return;
		}
	
		ball.current_y = lerp(
			ball.initial_y,
			ball.final_y,
			pattern //ease-out-bounce pattern
		);

		ball.shadow.width = lerp(
			2 * ball.circle.width, 
			ball.circle.width,
			pattern
		);
		ball.shadow.height = ball.shadow.width;

		set_ball_shadow_opacity(
			&ball.shadow,
			lerp(0, ball_shadow_top_color.a, pattern),
			lerp(ball_shadow_bottom_color.a / 1.7f, ball_shadow_bottom_color.a, pattern)
		);
	}
}


bool check_collision(Obstacle* obs, Ball* ball) {
	//if the player is jumping no collision can happen
	if (ball->is_jumping) return false;

	//Check if any external/top ball vertex is inside the obstacle's triangular base
	int vertex_num = 0;
	const float ball_width_half = ball->circle.width / 2.0f;
	const float ball_height_half = ball->circle.height / 2.0f;

	for (int i = 0; i < CIRCLE_TRIANGLES_NUM; i++) {
		PointRGBA ball_vertex_out_1 = {
			(ball->circle.points[vertex_num].x + 1) * ball_width_half + (ball->current_x - ball_width_half),
			(ball->circle.points[vertex_num].y + 1) * ball_height_half + (ball->current_y - ball_height_half),
			BALL_COLOR_TOP
		};
		PointRGBA ball_vertex_out_2 = {
			(ball->circle.points[vertex_num + 1].x + 1) * ball_width_half + (ball->current_x - ball_width_half),
			(ball->circle.points[vertex_num + 1].y + 1) * ball_height_half + (ball->current_y - ball_height_half),
			BALL_COLOR_TOP
		};
		
		if (
			point_is_in_triangle(ball_vertex_out_1, obs->points[0], obs->points[2], obs->points[5]) ||
			point_is_in_triangle(ball_vertex_out_2, obs->points[0], obs->points[2], obs->points[5])
		) {
			return true;
		}

		vertex_num += 3; //Skip inside/bottom ball's vertices
	}

	return false;
}

/*
 * Returns a float which is: 
 *  - <0 if p1 is on the left of the line between p2 and p3
 *  - >0 if p1 is on the right of the line between p2 and p3
 *  - =0 if p1 is on the line between p2 and p3
 */
float sign(PointRGBA p1, PointRGBA p2, PointRGBA p3) {
	return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

/*
 *   v1
 *  /  \
 * v2--v3
 * 
 * 3 lines passing through:
 *  - v1_v2 
 *  - v2_v3
 *  - v3_v1
 * 
 * Use the sign function above to determine the position of p wrt these 3 lines.
 * The result tells you if the point is inside or outside the triangle.
 */
bool point_is_in_triangle(PointRGBA p, PointRGBA v1, PointRGBA v2, PointRGBA v3) {
	float d1, d2, d3;
	bool has_neg, has_pos;

	d1 = sign(p, v1, v2);
	d2 = sign(p, v2, v3);
	d3 = sign(p, v3, v1);

	has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	return !(has_neg && has_pos);
}






void draw_overlay(float overlay_width, float overlay_height, Color color) {
	PointRGBA overlay_background_points[6] = {
		{ ((float)window_width - overlay_width) / 2.0f, ((float)window_height - overlay_height) / 2.0f, color.r, color.g, color.b, color.a },
		{ ((float)window_width + overlay_width) / 2.0f, ((float)window_height - overlay_height) / 2.0f, color.r, color.g, color.b, color.a },
		{ ((float)window_width - overlay_width) / 2.0f, ((float)window_height + overlay_height) / 2.0f, color.r, color.g, color.b, color.a },

		{ ((float)window_width + overlay_width) / 2.0f, ((float)window_height + overlay_height) / 2.0f, color.r, color.g, color.b, color.a },
		{ ((float)window_width - overlay_width) / 2.0f, ((float)window_height + overlay_height) / 2.0f, color.r, color.g, color.b, color.a },
		{ ((float)window_width + overlay_width) / 2.0f, ((float)window_height - overlay_height) / 2.0f, color.r, color.g, color.b, color.a }
	};

	//Draw the overlay background
	glBindVertexArray(overlay_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, overlay_VBO);
	glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(PointRGBA), &overlay_background_points[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(mat4(1.0)));
	vector<string> info_vec{};
	info_vec.push_back(" ");
	info_vec.push_back("Game Over ");
	info_vec.push_back("Score: " + std::to_string(game_score));
	info_vec.push_back(" ");
	HUD_Logger::get()->printInfo(info_vec, (float)window_width / 2.0f - overlay_width / 8.0f, ((float)window_height + overlay_height) / 2.0f);
}










/// ///////////////////////////////////////////////////////////////////////////////////
///									Gestione eventi
///////////////////////////////////////////////////////////////////////////////////////
void keyboardPressedEvent(unsigned char key, int x, int y) {
	bool is_pressed{};

	switch (key) {
		case LEFT_MOVE_KEY:
			is_pressed = is_key_pressed.find(key)->second;

			if (!is_pressed && ball.moving_line_index > 0) {
				is_key_pressed.insert_or_assign(key, true);

				ball.moving_line_index--;
				ball.initial_x = ball.current_x;
				ball.final_x = get_x_of_line_between_P1_P2(
					ball.current_y,
					moving_highway_lines[ball.moving_line_index].points[0],
					moving_highway_lines[ball.moving_line_index].points[1]
				);
				ball.movement_x_timestamp = -1;
			}
			break;
		case RIGHT_MOVE_KEY:
			is_pressed = is_key_pressed.find(key)->second;

			if (!is_pressed && ball.moving_line_index < MOVING_HIGHWAY_LINES_NUM - 1) {
				is_key_pressed.insert_or_assign(key, true);

				ball.moving_line_index++;
				ball.initial_x = ball.current_x;
				ball.final_x = get_x_of_line_between_P1_P2(
					ball.current_y,
					moving_highway_lines[ball.moving_line_index].points[0],
					moving_highway_lines[ball.moving_line_index].points[1]
				);
				ball.movement_x_timestamp = -1;
			}
			break; 
		case JUMP_MOVE_KEY:
			is_pressed = is_key_pressed.find(key)->second;
			
			if (!is_pressed && !ball.is_jumping) {
				is_key_pressed.insert_or_assign(key, true);

				ball.is_jumping = true;
				ball.initial_y = ball.current_y;
				ball.final_y = BALL_MAX_LINE_HEIGHT;
				ball.movement_y_timestamp = -1;
			}
			break;
		case 'k':
			if (__DEBUG__) {
				game_start_time = -1;
				game_speed = 1.0f;
				game_speed_acc = GAME_INITIAL_ACC;
			}
			break;
		case 'l':
			if (__DEBUG__) {
				game_speed_acc = 1.0f;
			}
			break;
		case 27:
			exit(0);
			break;
		default:
			break;
	}
}

void keyboardReleasedEvent(unsigned char key, int x, int y) {
	switch (key) {
	case LEFT_MOVE_KEY:
		is_key_pressed.insert_or_assign(key, false);
		break;
	case RIGHT_MOVE_KEY:
		is_key_pressed.insert_or_assign(key, false);
		break;
	case JUMP_MOVE_KEY:
		is_key_pressed.insert_or_assign(key, false);
		break;
	case 27:
		exit(0);
		break;
	default:
		break;
	}
}

void mouseEvent(int button, int state, int x, int y) {}

void update(int a) {
	glutPostRedisplay();

	if (!game_over) {
		glutTimerFunc(16, update, 0);
	}
}

void initShader(void) {
	GLenum ErrorCheckValue = glGetError();

	char* vertexShader = (char*)"vertexShader_C_M.glsl";
	char* fragmentShader = (char*)"fragmentShader_C_M.glsl";

	programId = ShaderMaker::createProgram(vertexShader, fragmentShader);
	glUseProgram(programId);

}

void init(void) {
	srand(get_current_time());

	//Init the ball
	ball.circle.width = 80;
	ball.circle.height = 80;
	ball.shadow.width = ball.circle.width;
	ball.shadow.height = ball.circle.height;
	ball.moving_line_index = 1;

	ball.current_x = float(window_width) / 2.0f;
	ball.current_y = BALL_MIN_LINE_HEIGHT;
	ball.final_x = ball.current_x;
	ball.final_y = ball.current_y;

	ball.movement_x_timestamp = -1;
	ball.movement_y_timestamp = -1;

	ball.is_jumping = false;

	create_circle(&ball.circle, ball_top_color, ball_bottom_color);
	create_circle(&ball.shadow, ball_shadow_top_color, ball_shadow_bottom_color);

	//Init the logger
	HUD_Logger::get()->set_text_box_height(130);
	HUD_Logger::get()->set_text_color(TRACK_LINES_COLOR);

	Projection = ortho(0.0f, float(window_width), 0.0f, float(window_height));
	MatProj = glGetUniformLocation(programId, "Projection");
	MatModel = glGetUniformLocation(programId, "Model");

	glClearColor(TRACK_COLOR_DARK); //Background color

	//Generate highway track VAO/VBO
	glGenVertexArrays(1, &track_VAO);
	glGenBuffers(1, &track_VBO);

	//Generate the static highway lines VAOs/VBOs
	for (int i = 0; i < STATIC_HIGHWAY_LINES_NUM; i++) {
		glGenVertexArrays(1, &static_highway_lines_VAOs[i]);
		glGenBuffers(1, &static_highway_lines_VBOs[i]);
	}

	//Generate the moving highway lines VAOs/VBOs
	for (int i = 0; i < MOVING_HIGHWAY_LINES_NUM; i++) {
		glGenVertexArrays(1, &moving_highway_lines_VAOs[i]);
		glGenBuffers(1, &moving_highway_lines_VBOs[i]);
	}

	//Generate the ball VAO/VBO
	glGenVertexArrays(1, &ball_VAO);
	glGenBuffers(1, &ball_VBO);

	//Generate the ball's shadow VAO/VBO
	glGenVertexArrays(1, &ball_shadow_VAO);
	glGenBuffers(1, &ball_shadow_VBO);

	//Generate all the obstacles VAOs/VBOs
	for (int i = 0; i < OBSTACLE_ON_SCREEN_MAX_NUM; i++) {
		glGenVertexArrays(1, &obstacles_VAOs[i]);
		glGenBuffers(1, &obstacles_VBOs[i]);
	}

	//Generate the overlay VAO/VBO
	glGenVertexArrays(1, &overlay_VAO);
	glGenBuffers(1, &overlay_VBO);

	glutSwapBuffers();
}

void drawScene(void) {
	glUniformMatrix4fv(MatProj, 1, GL_FALSE, value_ptr(Projection));
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(programId);

	// Handle output info layout 
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(mat4(1.0)));
	vector<string> score_vec{};
	score_vec.push_back(" ");
	score_vec.push_back("Score: " + std::to_string(game_score));
	score_vec.push_back(" ");
	score_vec.push_back("A: Move Left");
	score_vec.push_back("D: Move Right");
	score_vec.push_back("SPACE: Jump");
	HUD_Logger::get()->printInfo(score_vec, 20, window_height);

	update_ball_position();
	draw_highway();
	update_obstacles();
	draw_circle(ball.current_x, BALL_MIN_LINE_HEIGHT - ball.shadow.height / 2.0f, ball.shadow, ball_shadow_VAO, ball_shadow_VBO);
	draw_circle(ball.current_x, ball.current_y, ball.circle, ball_VAO, ball_VBO);
	update_game_speed();

	if (game_over) {
		Color overlay_background_color = { TRACK_LINES_COLOR };
		overlay_background_color.a = 0.88f;
		draw_overlay(350, 150, overlay_background_color);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glutSwapBuffers();
}

int main(int argc, char* argv[]) {
	glutInit(&argc, argv);

	glutInitContextVersion(4, 0);
	glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);

	glutInitWindowSize(window_width, window_height);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("2D Animazione e Gameplay - Highway Surfer - Cristian Davide Conte");
	glutDisplayFunc(drawScene);
	
	glutKeyboardFunc(keyboardPressedEvent);
	glutKeyboardUpFunc(keyboardReleasedEvent);
	
	//gestione animazione
	glutTimerFunc(16, update, 0); 
	glewExperimental = GL_TRUE;
	glewInit();

	initShader();
	init();

	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glutMainLoop();
}