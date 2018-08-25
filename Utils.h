#pragma once

struct Player;
struct Vector2;
struct Vector3;
struct WallLine;
struct WallData;

// *** GLOBAL DATA ***


extern SDL_Color colorRed; // used for text

// SDL vars:
extern TTF_Font *font;
extern SDL_Window *m_window;
extern SDL_Renderer *m_renderer;

extern SDL_Event event; // Used for tracking SDL events.


extern Player player;

// Walls must be defined in a clockwise 'winding order'
extern WallLine wall1;
extern WallLine wall2;
extern WallLine wall3;
extern WallLine wall4;
extern WallLine wall5;

// *** STRUCT DEFS ***

typedef struct Player
{
	float x;
	float y;
} Player;

typedef struct Vector2
{
	float x;
	float y;


	Vector2(float PassedX, float PassedY)
	{
		x = PassedX; y = PassedY;
	}

	Vector2()
	{
		x = 0.0; y = 0.0;
	}

} Vector2;

typedef struct Vector3
{
	float x;
	float y;
	float z;

	Vector3(float PassedX, float PassedY, float PassedZ)
	{
		x = PassedX; y = PassedY; z = PassedZ;
	}

	Vector3(double PassedX, double PassedY)
	{
		x = (float)PassedX; y = (float)PassedY; z = 0;
	}

	Vector3()
	{
		x = 0.0; y = 0.0; z = 0.0;
	}

	void Normalise()
	{
		float VectorLength = sqrt(x * x + y * y + z * z);
		x = x / VectorLength;
		y = y / VectorLength;
		z = z / VectorLength;
	};


} Vector3;

typedef struct WallData // Data struct with vertices needed to draw 2.5D wall
{
	float x1, x2;
	float y1a, y2a;
	float y1b, y2b;
} WallData;


typedef struct WallLine // Wall points (defined in absolute/top view) & wall colour 
{
	float PT1x;
	float PT1y;
	float PT2x;
	float PT2y;
	SDL_Color wallColor;
} WallLine;


// **** FUNCTION DECLARATIONS ****

#define min(a,b)             (((a) < (b)) ? (a) : (b)) // min: Choose smaller of two scalars.
#define max(a,b)             (((a) > (b)) ? (a) : (b)) // max: Choose greater of two scalars.

Vector2 Intersect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);
float Determinate(float x1, float y1, float x2, float y2);
void RenderWall(WallLine wallLine);
void RenderDebug(WallLine wallLine);
void DrawLineWithOffset(int x1, int y1, int x2, int y2, SDL_Point offset);
void DrawPixelWithOffset(float x, float y, SDL_Point offset);
// Offset functions make it easy to have multiple viewports

float Dot(Vector2 first, Vector2 second); // Calculate vector2 dot product.
Vector3 Cross(Vector3 first, Vector3 second); // Calculate Vector 3 cross product.
float Clamp(float Clampee, float MinVal, float MaxVal);


bool PlayerInBounds(Vector2 WallPt1, Vector2 WallPt2);
bool IsPlayerCollidingWithWall();

