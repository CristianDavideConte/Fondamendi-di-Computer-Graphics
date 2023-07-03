/*
 * Lab-01_students.c
 *
 *     This program draws straight lines connecting dots placed with mouse clicks.
 *
 * Usage:
 *   Left click to place a control point.
 *		Maximum number of control points allowed is currently set at 64.
 *	 Press "f" to remove the first control point
 *	 Press "l" to remove the last control point.
 *	 Press escape to exit.
 */


#include <iostream>
#include "ShaderMaker.h"
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <vector>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

static unsigned int programId;

unsigned int VAO;
unsigned int VBO;

unsigned int VAO_2;
unsigned int VBO_2;

using namespace glm;

const int MaxNumPts = 10;
const int MaxNumPtsCurve = 100;

float PointArray[MaxNumPts][2]{};
float CurveArray[MaxNumPtsCurve][2]{};
float PaintPointArray[3 * MaxNumPts][2]{}; //Array of control points to draw

int NumPts = 0;
int NumPaintPts = 0;
int NumCurvePts = 0;

// Window size in pixels
int	width = 500;
int	height = 500;

#define NoIndex -1
int pointToMoveIndex = -1;
float movingPrecision = 15.0f; // 15px scaled wrt window's width/height

#define ModeCasteljau 1
#define ModeCatmullRom 2
#define ModeAdaptiveSubdivision 3 
#define ModeCatmullRomCustomContinuity 4

#define MaxAdaptiveSubdivisionThreshold 0.5
#define AdaptiveSubdivisionThresholdStep 0.05
#define MinAdaptiveSubdivisionThreshold 0.005

int currentOperatingMode = ModeCasteljau;
bool showCatmullRomControlPoints = false;
int adaptiveSubdivisionCurrentStep = 0;
float adaptiveSubdivisionThreshold = 0.1;

float TCBTension = 0.0f;
float TCBContinuity = 0.0f; //C1
float TCBBias = 0.0f;
float TCBAlpha = 0.0f; //Needed for G1 curves

/* Prototypes */
void addNewPoint(float x, float y);
int main(int argc, char** argv);
void removeFirstPoint();
void removeLastPoint();
void init(void);

void printOperatingMode() {
	if (currentOperatingMode == ModeCasteljau) {
		cout << "ModeCasteljau\n\n";
	}
	else if (currentOperatingMode == ModeCatmullRom) {
		cout << "ModeCatmullRom\n";
		cout << "Right click to show/hide the additional control points\n\n";
	}
	else if (currentOperatingMode == ModeAdaptiveSubdivision){
		cout << "ModeAdaptiveSubdivision\n";
		cout << "Use the mouse wheel to change the subdivision threshold\n\n";
	}
	else if (currentOperatingMode == ModeCatmullRomCustomContinuity) {
		cout << "ModeCatmullRomCustomContinuity\n";
		cout << "Press T for C0 continuity\n";
		cout << "Press C for C1 continuity\n";
		cout << "Press G for G1 continuity\n";
		cout << "Right click to show/hide the additional control points\n\n";
	}
}

void copyPoint(float pointDst[2], float pointSrc[2]) {
	pointDst[0] = pointSrc[0];
	pointDst[1] = pointSrc[1];
}

void myKeyboardFunc(unsigned char key, int x, int y)
{
	switch (key) {
	case 'f':
		removeFirstPoint();
		glutPostRedisplay();
		break;
	case 'l':
		removeLastPoint();
		glutPostRedisplay();
		break;
	case 't': //C0
		TCBContinuity = 0.5f;
		TCBAlpha = 0.0f;
		glutPostRedisplay();
		break;
	case 'c': //C1
		TCBContinuity = 0.0f;
		TCBAlpha = 0.0f;
		glutPostRedisplay();
		break;
	case 'g': //G1
		TCBContinuity = 0.0f;
		TCBAlpha = 0.5f;
		glutPostRedisplay();
		break;
	case 27:			// Escape key
		exit(0);
		break;
	}
}


// Looks for a control point with the same coordinates as the input ones (+/- the precision in px)
int searchPointInPointArray(float x, float y, float precision) {
	for (int i = 0; i < NumPts; i++) {
		bool xInRange = PointArray[i][0] - precision <= x && PointArray[i][0] + precision >= x;
		bool yInRange = PointArray[i][1] - precision <= y && PointArray[i][1] + precision >= y;
		
		if (xInRange && yInRange) {
			return i;
		}
	}

	return NoIndex;
}

void removeFirstPoint() {
	int i;
	if (NumPts > 0) {
		// Remove the first point, slide the rest down
		NumPts--;
		for (i = 0; i < NumPts; i++) {
			PointArray[i][0] = PointArray[i + 1][0];
			PointArray[i][1] = PointArray[i + 1][1];
		}
	}
}
void resizeWindow(int w, int h)
{
	height = (h > 1) ? h : 2;
	width = (w > 1) ? w : 2;
	gluOrtho2D(-1.0f, 1.0f, -1.0f, 1.0f);
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
}

// Left button presses place a new control point.
void myMouseFunc(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		// (x,y) viewport(0,width)x(0,height)   -->   (xPos,yPos) window(-1,1)x(-1,1)
		float xPos = -1.0f + ((float)x) * 2 / ((float)(width));
		float yPos = -1.0f + ((float)(height - y)) * 2 / ((float)(height));

		pointToMoveIndex = searchPointInPointArray(xPos, yPos, movingPrecision / width); // [x,y]

		// If no drag operation has been triggered, 
		// add a new control point
		if (pointToMoveIndex == NoIndex) {
			addNewPoint(xPos, yPos);
			glutPostRedisplay();
		}
	} else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
		pointToMoveIndex = -1;
	}
	else if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN) {
		// Change operating mode
		if (currentOperatingMode == ModeCasteljau) {
			currentOperatingMode = ModeCatmullRom;
		} 
		else if (currentOperatingMode == ModeCatmullRom) {
			currentOperatingMode = ModeAdaptiveSubdivision;
		}
		else if (currentOperatingMode == ModeAdaptiveSubdivision) {
			currentOperatingMode = ModeCatmullRomCustomContinuity;
		}
		else if (currentOperatingMode == ModeCatmullRomCustomContinuity) {
			currentOperatingMode = ModeCasteljau;
		}

		cout << "Mode changed to: ";
		printOperatingMode();
		
		glutPostRedisplay();
	}
	else if(button == GLUT_RIGHT_BUTTON && state == GLUT_UP && (currentOperatingMode == ModeCatmullRom || currentOperatingMode == ModeCatmullRomCustomContinuity)){
		showCatmullRomControlPoints = !showCatmullRomControlPoints;
		glutPostRedisplay();
	}
	else if (button == 3 && currentOperatingMode == ModeAdaptiveSubdivision) { //Mousewheel scroll up
		adaptiveSubdivisionThreshold = std::min(MaxAdaptiveSubdivisionThreshold, adaptiveSubdivisionThreshold + AdaptiveSubdivisionThresholdStep);
		glutPostRedisplay();
	}
	else if (button == 4 && currentOperatingMode == ModeAdaptiveSubdivision) { //Mousewheel scroll down
		adaptiveSubdivisionThreshold = std::max(MinAdaptiveSubdivisionThreshold, adaptiveSubdivisionThreshold - AdaptiveSubdivisionThresholdStep);
		glutPostRedisplay();
	}
}

void myMotionFunc(int x, int y) {
	// No drag operation was previously triggered
	if (pointToMoveIndex < 0) return;

	// Scale the mouse coordinates according to the window's width/height
	float xPos = -1.0f + ((float)x) * 2 / ((float)(width));
	float yPos = -1.0f + ((float)(height - y)) * 2 / ((float)(height));

	// Keep the coordinates within the window's boudaries
	float newX = std::min(1.0f, std::max(-1.0f, xPos));
	float newY = std::min(1.0f, std::max(-1.0f, yPos));

	// Move the control point according to the mouse movement
	PointArray[pointToMoveIndex][0] = newX;
	PointArray[pointToMoveIndex][1] = newY;

	glutPostRedisplay();
}

// Add a new point to the end of the list.  
// Remove the first point in the list if too many points.
void removeLastPoint() {
	if (NumPts > 0) {
		NumPts--;
	}
}

// Add a new point to the end of the list.  
// Remove the first point in the list if too many points.
void addNewPoint(float x, float y) {
	if (NumPts >= MaxNumPts) {
		removeFirstPoint();
	}

	PointArray[NumPts][0] = x;
	PointArray[NumPts][1] = y;
	NumPts++;
}
void initShader(void)
{
	GLenum ErrorCheckValue = glGetError();

	char* vertexShader = (char*)"vertexShader.glsl";
	char* fragmentShader = (char*)"fragmentShader.glsl";

	programId = ShaderMaker::createProgram(vertexShader, fragmentShader);
	glUseProgram(programId);

}

// Linear interpolation method
float Lerp(float t, float a, float b) {
	return (1 - t) * a + t * b;
}

// De casteljau method used to fill the 
// CurveArray for the bezier curve
float * deCasteljau(float t, float initialPoints[MaxNumPts][2], int numPoints) {
	/*
	 * Algoritmo: 
	 * Dati n punti di controllo,
	 * Utilizzando il parametro t,
	 * Usare n-1 volte lerp per produrre i nuovi n-1 punti di controllo,
	 * Iterare fino ad arrivare ad avere solo 1 punto,
	 * Quel punto è il valore della curva di bezier con i punti di controllo iniziali in t
	 */
	float iterArray[MaxNumPts][2]{};
	int iterArrayLen = numPoints - 1;
	
	// Deep copy the initialPoints array
	for (int i = 0; i < numPoints; i++) {
		copyPoint(iterArray[i], initialPoints[i]);
	}

	while (iterArrayLen > 0) {
		for (int i = 0; i < iterArrayLen; i++) {
			// x-coordinate lerp
			iterArray[i][0] = Lerp(
				t,
				iterArray[i][0],
				iterArray[i + 1][0]
			);

			// y-coordinate lerp
			iterArray[i][1] = Lerp(
				t,
				iterArray[i][1],
				iterArray[i + 1][1]
			);
		}
		iterArrayLen--;
	}

	return iterArray[0]; // [x, y] coordinates of the bezier curve in t
}

//Estimate the m values of Catmull-Rom
void estimateDerivative(float points[MaxNumPts][2], int index, int length, float m[2]) {
	float* prevPoint{};
	float* nextPoint{};
	float h = 1.0f;

	if (index == 0) { //Initial point = forward difference 
		prevPoint = points[index];
		nextPoint = points[index + 1];
	}
	else if (index == length - 1) { //Last point = backward difference
		prevPoint = points[index - 1];
		nextPoint = points[index];
	}
	else { //Point in the middle of the curve = central difference
		prevPoint = points[index - 1];
		nextPoint = points[index + 1];
		h = 2.0f;
	}

	m[0] = (nextPoint[0] - prevPoint[0]) / h; // mx
	m[1] = (nextPoint[1] - prevPoint[1]) / h; // my
}

//Estimate the L value of TCB Spline
void estimateTCBLeftTangent(float points[MaxNumPts][2], int index, int length, float tension, float continuity, float bias, float alpha, float L[2]) {
	float* prevPoint{};
	float* nextPoint{};

	if (index == 0) { //Initial point = forward difference 
		prevPoint = points[index];
		nextPoint = points[index + 1];
	}
	else if (index == length - 1) { //Last point = backward difference
		prevPoint = points[index - 1];
		nextPoint = points[index];
	}
	else { //Point in the middle of the curve = central difference
		prevPoint = points[index - 1];
		nextPoint = points[index + 1];
	}

	L[0] = (1 - tension + alpha) * (1 - continuity) * (1 + bias) / 2.0f * (points[index][0] - prevPoint[0]) +
		   (1 - tension + alpha) * (1 + continuity) * (1 - bias) / 2.0f * (nextPoint[0] - points[index][0]);

	L[1] = (1 - tension + alpha) * (1 - continuity) * (1 + bias) / 2.0f * (points[index][1] - prevPoint[1]) +
		   (1 - tension + alpha) * (1 + continuity) * (1 - bias) / 2.0f * (nextPoint[1] - points[index][1]);
}

//Estimate the R value of TCB Spline
void estimateTCBRightTangent(float points[MaxNumPts][2], int index, int length, float tension, float continuity, float bias, float alpha, float R[2]) {
	float* prevPoint{};
	float* nextPoint{};

	if (index == 0) { //Initial point = forward difference 
		prevPoint = points[index];
		nextPoint = points[index + 1];
	}
	else if (index == length - 1) { //Last point = backward difference
		prevPoint = points[index - 1];
		nextPoint = points[index];
	}
	else { //Point in the middle of the curve = central difference
		prevPoint = points[index - 1];
		nextPoint = points[index + 1];
	}

	R[0] = (1 - tension - alpha) * (1 - continuity) * (1 + bias) / 2.0f * (points[index][0] - prevPoint[0]) +
		   (1 - tension - alpha) * (1 + continuity) * (1 - bias) / 2.0f * (nextPoint[0] - points[index][0]);

	R[1] = (1 - tension - alpha) * (1 + continuity) * (1 + bias) / 2.0f * (points[index][1] - prevPoint[1]) +
		   (1 - tension - alpha) * (1 - continuity) * (1 - bias) / 2.0f * (nextPoint[1] - points[index][1]);
}


void catmullRom(float p1[2], float p4[2], float m1[2], float m4[2], float controlPoints[MaxNumPts][2]) {
	/*
	 * Algoritmo:
	 * Dati 2 punti
	 * Stimare le derivate in quei punti (m) con:
	 * - differenze centrali ((pi+1 - pi-1)/2) se esistono il punto precedente/successivo
	 * - differenze destre ((pi+1 - pi)/2) altrimenti
	 * Usare gli m calcolati per stimare altri 2 control points
	 * Usare deCasteljau per disegnare la curva avente i 4 punti ottenuti
	 */
	float p2[2] = {0,0};
	float p3[2] = {0,0};
	
	//P1+
	p2[0] = p1[0] + (1.0f / 3.0f) * m1[0];
	p2[1] = p1[1] + (1.0f / 3.0f) * m1[1];

	//P4-
	p3[0] = p4[0] - (1.0f / 3.0f) * m4[0];
	p3[1] = p4[1] - (1.0f / 3.0f) * m4[1];
	
	copyPoint(controlPoints[0], p1); //P1
	copyPoint(controlPoints[1], p2); //P2 (P1+)
	copyPoint(controlPoints[2], p3); //P3 (P4-)
	copyPoint(controlPoints[3], p4); //P4
}

float pointRectDistance(float point[2], float rect_P0[2], float rect_P1[2]) {
	//Distanza punto-retta = |Ax0 + By0 + C| / sqrt(A^2 + B^2)
	//(x - x1) / (x2 - x1) = (y - y1) / (y2 - y1)
	//(x - x1) * (y2 - y1) = (y - y1) * (x2 - x1)
	//(y2 - y1) * x - (y2 - y1) * x1 = (x2 - x1) * y - (x2 - x1) * y1
	//(y2 - y1) * x - (x2 - x1) * y - (y2 - y1) * x1 + (x2 - x1) * y1 = 0
	//A = (y2 - y1)
	//B = -(x2 - x1) = (x1 - x2)
	//C = - (y2 - y1) * x1 + (x2 - x1) * y1 = (y1 - y2) * x1 + (x2 - x1) * y1

	float x0 = point[0];
	float y0 = point[1];

	float x1 = rect_P0[0];
	float y1 = rect_P0[1];
	
	float x2 = rect_P1[0];
	float y2 = rect_P1[1];

	float A = y2 - y1;
	float B = x1 - x2;
	float C = (y1 - y2) * x1 + (x2 - x1) * y1;

	float distance = std::abs(A * x0 + B * y0 + C) / std::sqrt(A * A + B * B);
	
	return distance;
}

void adaptiveSubdivision(float t, float initialPoints[MaxNumPts][2], int numPoints, float threashold) {
	/*
	 * Algoritmo:
	 * Usare DeCasteljau con t = 0.5 per dividere la curva iniziale in 2 sottocurve.
	 * DeCasteljau(t = 0.5) sarà il l'ultimo control point della sottocurva di sinistra ed il primo di quella di destra.
	 * I rimanenti 4 (2 per sotto-parte) sono direttamente forniti durante le iterazioni di DeCasteljau.
	 * Una volta che la distanza tra i control point centrali e la retta passante tra quelli esterni è 
	 * inferiore ad una certa soglia, disegnare il segmento.+
	 * 
	 * Nota: una soglia più bassa garantisce una curva più smooth, ma peggiora le performance.
	 */
	float iterArray[MaxNumPts][2]{};
	int iterArrayLen = numPoints - 1;

	// Test planarità sui control point interni
	bool test_planarita = true;
	for (int i = 0; i < numPoints; i++) {
		if (
			pointRectDistance(
				initialPoints[i], //point 
				initialPoints[0], //rect p1
				initialPoints[iterArrayLen] //rect p2
			) > threashold
		) {
			test_planarita = false; 
			break;
		}
	}

	if (test_planarita) {
		copyPoint(
			CurveArray[adaptiveSubdivisionCurrentStep],
			initialPoints[0]
		);

		copyPoint(
			CurveArray[adaptiveSubdivisionCurrentStep + 1],
			initialPoints[iterArrayLen]
		);

		adaptiveSubdivisionCurrentStep += 2;
		return;
	}

	float adaptivePointsLeft[MaxNumPts][2]{};
	float adaptivePointsRight[MaxNumPts][2]{};

	// P0_left
	copyPoint(
		adaptivePointsLeft[0],
		initialPoints[0]
	);

	// Pn_right
	copyPoint(
		adaptivePointsRight[iterArrayLen],
		initialPoints[iterArrayLen]
	);

	// Deep copy the initialPoints array
	for (int i = 0; i < numPoints; i++) {
		copyPoint(iterArray[i], initialPoints[i]);
	}

	// DeCasteljau
	while (iterArrayLen > 0) {
		for (int i = 0; i < iterArrayLen; i++) {

			// x-coordinate lerp
			iterArray[i][0] = Lerp(
				t,
				iterArray[i][0],
				iterArray[i + 1][0]
			);

			// y-coordinate lerp
			iterArray[i][1] = Lerp(
				t,
				iterArray[i][1],
				iterArray[i + 1][1]
			);
			
			// Fill the left control points
			if (i == 0) {
				//Pi_left
				copyPoint(
					adaptivePointsLeft[numPoints - iterArrayLen],
					iterArray[i]
				);
			}

			// Fill the right control points
			if (i == iterArrayLen - 1) {
				//Pi_right
				copyPoint(
					adaptivePointsRight[iterArrayLen - 1],
					iterArray[i]
				);
			}
		}
		iterArrayLen--;
	}

	adaptiveSubdivision(t, adaptivePointsLeft, numPoints, threashold);
	adaptiveSubdivision(t, adaptivePointsRight, numPoints, threashold);
}

void init(void)
{
	// VAO for control polygon
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// VAO for curve
	glGenVertexArrays(1, &VAO_2);
	glBindVertexArray(VAO_2);
	glGenBuffers(1, &VBO_2);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_2);

	// Background color
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glViewport(0, 0, 500, 500);

	cout << "\n\n";
	printOperatingMode();
	cout << "\nPress MIDDLE_MOUSE_BUTTON to change mode\n\n";
}

void drawScene(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	if (NumPts > 1) {
		NumCurvePts = MaxNumPtsCurve;

		if (currentOperatingMode == ModeCasteljau) {
			// Make sure the curve is smooth
			int numSteps = MaxNumPtsCurve;

			// Draw curve
			for (int i = 0; i < numSteps; i++) {
				float t = (float)i / (numSteps - 1);

				copyPoint(
					CurveArray[i],
					deCasteljau(t, PointArray, NumPts)
				);
			}
		}
		else if (currentOperatingMode == ModeCatmullRom) {
			int curveArrayTotalDrawingSteps = 0;

			if (showCatmullRomControlPoints) {
				NumPaintPts = 0;
			}

			for (int i = 0; i < NumPts - 1; i++) {
				float m1[2] = { 0,0 };
				float m4[2] = { 0,0 };
				float controlPoints[MaxNumPts][2] = { 0,0,0,0 };
				
				estimateDerivative(PointArray, i,     NumPts, m1);
				estimateDerivative(PointArray, i + 1, NumPts, m4);

				catmullRom(
					PointArray[i],
					PointArray[i + 1],
					m1,
					m4,
					controlPoints
				);

				// Make sure the curve is smooth
				const int numSteps = i == NumPts - 2 ? 
					MaxNumPtsCurve - curveArrayTotalDrawingSteps : 
					MaxNumPtsCurve / (NumPts - 1);

				// Draw curve
				for (int j = 0; j < numSteps; j++) {
					float t = (float)j / (numSteps - 1);

					copyPoint(
						CurveArray[curveArrayTotalDrawingSteps + j],
						deCasteljau(t, controlPoints, 4)
					);
				}
				curveArrayTotalDrawingSteps += numSteps;

				if (!showCatmullRomControlPoints) continue;

				// Copy the user's + additional control points to the paint array
				int upperBound = i == NumPts - 2 ? 4 : 3;
				for (int j = 0; j < upperBound; j++) {
					PaintPointArray[NumPaintPts + j][0] = controlPoints[j][0];
					PaintPointArray[NumPaintPts + j][1] = controlPoints[j][1];
				}
				NumPaintPts += upperBound;
			}
		}
		else if (currentOperatingMode == ModeAdaptiveSubdivision) {
			adaptiveSubdivisionCurrentStep = 0;
			adaptiveSubdivision(0.5, PointArray, NumPts, adaptiveSubdivisionThreshold);
			
			NumCurvePts = adaptiveSubdivisionCurrentStep;
		}
		else if (currentOperatingMode == ModeCatmullRomCustomContinuity) {
			int curveArrayTotalDrawingSteps = 0;

			if (showCatmullRomControlPoints) {
				NumPaintPts = 0;
			}

			//Stesso identico algoritmo di CatmullRom, ma qui gli m sono calcolati secondo le formule
			//di L e R di una TCB spline (così che l'utente possa cambiare la continuità a piacimento).
			for (int i = 0; i < NumPts - 1; i++) {
				float L[2] = { 0,0 };
				float R[2] = { 0,0 };
				float controlPoints[MaxNumPts][2] = { 0,0,0,0 };

				estimateTCBLeftTangent(PointArray, i, NumPts, TCBTension, TCBContinuity, TCBBias, TCBAlpha, L);
				estimateTCBRightTangent(PointArray, i + 1, NumPts, TCBTension, TCBContinuity, TCBBias, TCBAlpha, R);

				catmullRom(
					PointArray[i],
					PointArray[i + 1],
					L,
					R,
					controlPoints
				);

				// Make sure the curve is smooth
				const int numSteps = i == NumPts - 2 ?
					MaxNumPtsCurve - curveArrayTotalDrawingSteps :
					MaxNumPtsCurve / (NumPts - 1);

				// Draw curve
				for (int j = 0; j < numSteps; j++) {
					float t = (float)j / (numSteps - 1);

					copyPoint(
						CurveArray[curveArrayTotalDrawingSteps + j],
						deCasteljau(t, controlPoints, 4)
					);
				}
				curveArrayTotalDrawingSteps += numSteps;

				if (!showCatmullRomControlPoints) continue;

				// Copy the user's + additional control points to the paint array
				int upperBound = i == NumPts - 2 ? 4 : 3;
				for (int j = 0; j < upperBound; j++) {
					PaintPointArray[NumPaintPts + j][0] = controlPoints[j][0];
					PaintPointArray[NumPaintPts + j][1] = controlPoints[j][1];
				}
				NumPaintPts += upperBound;
			}
		}
	}

	// Copy the user's control points to the paint array 
	if ((currentOperatingMode != ModeCatmullRom && currentOperatingMode != ModeCatmullRomCustomContinuity) || !showCatmullRomControlPoints) {
		for (int i = 0; i < NumPts; i++) {
			PaintPointArray[i][0] = PointArray[i][0];
			PaintPointArray[i][1] = PointArray[i][1];
		}
		NumPaintPts = NumPts; 
	}

	// Draw control polygon
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(PaintPointArray), &PaintPointArray[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	
	// Draw the control points CP
	glPointSize(6.0);
	glDrawArrays(GL_POINTS, 0, NumPaintPts);
	
	// Draw the line segments between CP
	glLineWidth(2.0);
	glDrawArrays(GL_LINE_STRIP, 0, NumPaintPts);
	glBindVertexArray(0);

	// Draw the bezier polygon 
	glBindVertexArray(VAO_2);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(CurveArray), &CurveArray[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	 
	// Draw the bezier curve
	glLineWidth(2.0);
	glDrawArrays(GL_LINE_STRIP, 0, NumCurvePts);
	glBindVertexArray(0);

	// Swap the drawing buffers
	glutSwapBuffers();
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);

	glutInitContextVersion(4, 0);
	glutInitContextProfile(GLUT_CORE_PROFILE);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);

	glutInitWindowSize(width, height);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Disegno di Curve di Bézier - Cristian Davide Conte");

	glutDisplayFunc(drawScene);
	glutReshapeFunc(resizeWindow);
	glutKeyboardFunc(myKeyboardFunc);
	glutMouseFunc(myMouseFunc);
	glutMotionFunc(myMotionFunc);

	glewExperimental = GL_TRUE;
	glewInit();

	initShader();
	init();

	glutMainLoop();
}
