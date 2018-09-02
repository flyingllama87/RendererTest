#include <string>
#include <cmath>
#include <SDL.h>
#include <SDL_ttf.h>
#include "Utils.h"



// *** GLOBAL DATA ***

SDL_Color colorRed = { 255, 0, 0, 255 }; // used for text

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

float Clamp(float Clampee, float MinVal, float MaxVal)
{
	if (Clampee < MinVal)
		Clampee = MinVal;
	if (Clampee > MaxVal)
		Clampee = MaxVal;
	return Clampee;
}

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

Vector2 Lerp(Vector2 Start, Vector2 End, float t)
{
	Vector2 ReturnVector;
	t = Clamp(t, 0.0, 1.0);
	ReturnVector.x = Start.x + ((End.x - Start.x) * t);
	ReturnVector.y = Start.y + ((End.y - Start.y) * t);

	return ReturnVector;
}