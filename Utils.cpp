#include <string>
#include <cmath>
#include <SDL.h>
#include <SDL_ttf.h>
#include "Utils.h"



// *** GLOBAL DATA ***

SDL_Color colorRed = { 255, 0, 0, 255 }; // used for debug text

// SDL vars:
TTF_Font *font;
SDL_Window *g_window;
SDL_Renderer *g_renderer;
SDL_Surface *g_surface;
SDL_Event event; // Used for event SDL events.



// **** UTILITY FUNCTIONS ****


// Offset functions make it easy to have multiple viewports
void DrawLineWithOffset(int x1, int y1, int x2, int y2, SDL_Point offset)
{
	SDL_RenderDrawLine(g_renderer, x1 + offset.x, y1 + offset.y, x2 + offset.x, y2 + offset.y);
}

void DrawLineWithScaleAndOffset(float scale, float x1, float y1, float x2, float y2, SDL_Point offset)
{
	SDL_RenderDrawLine(g_renderer, (int)(scale * x1) + offset.x, (int)(scale * y1) + offset.y, (int)(scale * x2) + offset.x, (int)(scale * y2) + offset.y);
}

void DrawPixelWithOffset(float x, float y, SDL_Point offset)
{
	SDL_RenderDrawPoint(g_renderer, x + offset.x, y + offset.y);
}

void DrawPixelWithScaleAndOffset(float scale, float x, float y, SDL_Point offset)
{
	SDL_RenderDrawPoint(g_renderer, (int)(scale * x) + offset.x, (int)(scale * y) + offset.y);
}

// *** GENERIC MATH FUNCTIONS ***

// Provides determinate.
float Determinate(float x1, float y1, float x2, float y2)
{
	return x1 * y2 - x2 * y1;
}

// Returns point of intersection however this can happen beyond where the two points of the line segments are defined.  Uses 'Cramer's method' to calculate the intersection.  See 'https://en.wikipedia.org/wiki/Line–line_intersection' for more.
Vector2 Intersect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
	float x = Determinate(x1, y1, x2, y2);
	float y = Determinate(x3, y3, x4, y4);

	float determinant = Determinate(x1 - x2, y1 - y2, x3 - x4, y3 - y4);
	Vector2 RetPoint;

	RetPoint.x = Determinate(x, x1 - x2, y, x3 - x4) / determinant;
	RetPoint.y = Determinate(x, y1 - y2, y, y3 - y4) / determinant;

	return RetPoint;
}




// Returns point of intersection between two line segments. Returns zero vector if lines segs don't intersect.  See 'https://en.wikipedia.org/wiki/Line–line_intersection' for more.
Vector2 IntersectLineSegs(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{

	Vector2 RetPoint = { 0.0f, 0.0f };

	float denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

	if (denominator == 0)
		return RetPoint; // return empty vector as lines are collinear.	

	float t =((x1 - x3) * (y3 - y4) - (y1 - y3) *  (x3 - x4)) / denominator;

	float u = -(((x1 - x2) * (y1 - y3) - (y1 - y2 ) * (x1 - x3)) / denominator);

	if (t > 1.0 || t < 0.0 || u > 1.0 || u < 0.0)
		return RetPoint; // return empty vector as line's don't intersect.


	// RetPoint.x = (x3 + u * (x4 - x3));
	// RetPoint.y = (y3 + u * (y4 - y3));

	RetPoint.x = x1 + t * (x2 - x1);
	RetPoint.y = y1 + t * (y2 - y1);

	return RetPoint;
} 


float Dot(Vector2 first, Vector2 second) // Calculate vector2 dot product.
{
	return first.x * second.x + first.y * second.y;
};

Vector3 Cross(Vector3 first, Vector3 second)
{
	Vector3 ReturnVector;
	ReturnVector.x = first.y * second.z - first.z * second.y;
	ReturnVector.y = first.z * second.x - first.x * second.z;
	ReturnVector.z = first.x * second.y - first.y * second.x;

	return ReturnVector;
}

/*float Clamp(float Clampee, float MinVal, float MaxVal) // Moved to macro so it works for both floats and ints inline
{
	if (Clampee < MinVal)
		Clampee = MinVal;
	if (Clampee > MaxVal)
		Clampee = MaxVal;
	return Clampee;
}*/

// ** GENERIC PHYSICS FUNCTIONS **

bool IsPlayerCollidingWithWall()
{

	for (auto& wall : AllWalls)
	{
		if (!PlayerInBounds(Vector2(wall.PT1x, wall.PT1y), Vector2(wall.PT2x, wall.PT2y)))
			return true;
	}

	return false;

}


bool PlayerInBounds(Vector2 WallPt1, Vector2 WallPt2) // Figure out which side of wall player is on.  See https://stackoverflow.com/questions/1560492/how-to-tell-whether-a-point-is-to-the-right-or-left-side-of-a-line#3461533
{
	// Get determinate of matrix made 2 vectors.  1 = WallPt1 - WallPt2, 2 = WallPt1 - Player. 
	double det = (WallPt1.x - WallPt2.x) * (WallPt1.y - player.y) - (WallPt1.x - player.x) * (WallPt1.y - WallPt2.y);
	if (det > 0.0)
		return true;
	else
		return false;
}

bool LineSide(Vector2 LinePt1, Vector2 LinePt2, Vector2 Point) // Figure out which side of a line a point is on.  See https://stackoverflow.com/questions/1560492/how-to-tell-whether-a-point-is-to-the-right-or-left-side-of-a-line#3461533
{
	// Get determinate of matrix made 2 vectors.
	double det = (LinePt1.x - LinePt2.x) * (LinePt1.y - Point.y) - (LinePt1.x - Point.x) * (LinePt1.y - LinePt2.y);
	if (det > 0.0)
		return true;
	else
		return false;
}

Vector2 Lerp(Vector2 Start, Vector2 End, float t)
{
	Vector2 ReturnVector;
	t = Clamp(t, 0.0, 1.0);
	ReturnVector.x = Start.x + ((End.x - Start.x) * t);
	ReturnVector.y = Start.y + ((End.y - Start.y) * t);

	return ReturnVector;
}






////////////////////////////////////////////////


// Verbose/expensive method of calculating line segment intersection.  Possibly most accurate model.

/*
Vector2 lineSegmentIntersection(
double Ax, double Ay,
double Bx, double By,
double Cx, double Cy,
double Dx, double Dy) {

Vector2 ReturnVector = { 0.0f, 0.0f };

double  distAB, theCos, theSin, newX, ABpos;

//  Fail if either line segment is zero-length.
if (Ax == Bx && Ay == By || Cx == Dx && Cy == Dy) return ReturnVector;

//  Fail if the segments share an end-point.
if (Ax == Cx && Ay == Cy || Bx == Cx && By == Cy
|| Ax == Dx && Ay == Dy || Bx == Dx && By == Dy) {
return ReturnVector;
}

//  (1) Translate the system so that point A is on the origin.
Bx -= Ax; By -= Ay;
Cx -= Ax; Cy -= Ay;
Dx -= Ax; Dy -= Ay;

//  Discover the length of segment A-B.
distAB = sqrt(Bx*Bx + By * By);

//  (2) Rotate the system so that point B is on the positive X axis.
theCos = Bx / distAB;
theSin = By / distAB;
newX = Cx * theCos + Cy * theSin;
Cy = Cy * theCos - Cx * theSin; Cx = newX;
newX = Dx * theCos + Dy * theSin;
Dy = Dy * theCos - Dx * theSin; Dx = newX;

//  Fail if segment C-D doesn't cross line A-B.
if (Cy<0. && Dy<0. || Cy >= 0. && Dy >= 0.) return ReturnVector;

//  (3) Discover the position of the intersection point along line A-B.
ABpos = Dx + (Cx - Dx)*Dy / (Dy - Cy);

//  Fail if segment C-D crosses line A-B outside of segment A-B.
if (ABpos<0. || ABpos>distAB) return ReturnVector;

//  (4) Apply the discovered position to line A-B in the original coordinate system.
ReturnVector.x = Ax + ABpos * theCos;
ReturnVector.y = Ay + ABpos * theSin;

//  Success.
return ReturnVector;
}
*/



/* Less concise but clearer algo than the function below.
Vector2 IntersectLineSegs(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{

Vector2 RetPoint = { 0.0f, 0.0f };

Vector2 qp = { x3 - x1, y3 - y1 };
Vector2 r = { x2 - x1, y2 - y1 };
Vector2 s = { x4 - x3, y4 - y3 };


float qpXr = Determinate(qp.x, qp.y, r.x, r.y);
float qpXs = Determinate(qp.x, qp.y, s.x, s.y);
float rXs = Determinate(r.x, r.y, s.x, s.y);


if (qpXr == 0.0f || rXs == 0.0f)
return RetPoint; // return empty vector as lines are collinear.

float t = qpXs / rXs;
float u = qpXr / rXs;

if (t > 1.0 || t < 0.0 || u > 1.0 || u < 0.0)
return RetPoint; // return empty vector as line's don't intersect.


//	RetPoint.x = (x3 + u * (x4 - x3));
//	RetPoint.y = (y3 + u * (y4 - y3));

RetPoint.x = (x1 + t * (x2 - x1));
RetPoint.y = (y1 + t * (y2 - y1));

return RetPoint;
} */
