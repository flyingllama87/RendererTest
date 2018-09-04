#include <string>
#include <cmath>
#include <SDL.h>
#include <SDL_ttf.h>
#include "Utils.h"
#include <cassert>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WindowWidth 1680
#define WindowHeight 1050
#define HalfWindowHeight WindowHeight/2
#define HalfWindowWidth WindowWidth/2

#define hfov (0.73f*WindowHeight)  // Affects the horizontal field of vision
#define vfov (.2f*WindowHeight)    // Affects the vertical field of vision


/*
WARNING:  This code is messy & purely PoC.  You've been warned.

TO DO:
- Code clean up & break program into separate files.
- Better clipping.
- Make mobile friendly?

FEATURES TO IMPLEMENT:
- Optomisation - Draw rects where possible, remove divides (calc reciprocal once?), avoid squares,
- interpolate between wall light values
- Draw Light box instead of a single pixel.
- Texture Mapping
- Per VLine Lighting
- Quadratic lighting falloff? (y = -x^2 + c)?
- Reading sectors from text file (duke3d/build format?)
- Wall Height.
- Jumping.
- BSP/Sectors/Portals?
- Better FOV understanding?

*/



// **** GLOBAL DATA ****

bool P1OnLeftSideOfClipPlane = false;
bool P2OnLeftSideOfClipPlane = false;
bool P1OnRightSideOfClipPlane = false;
bool P2OnRightSideOfClipPlane = false;


int TickCount = 0, PreviousTickCount, fps, TickDelta; // used for fps monitor

bool ContinueGame = true; // used as a conditional for exiting the app.

const int ViewWidth = 256;
const int ViewHeight = 256;
const int HalfViewWidth = ViewWidth / 2;
const int HalfViewHeight = ViewHeight / 2;

// PLAYER DEFS

float CrouchingHeight = 6 * HalfWindowHeight; // Crouching Height
float StandingHeight = 10 * HalfWindowHeight; // standing height.

float PlayerHeight = StandingHeight; // Start standing.
Player player = { 25.0, 25.0 }; // Could be moved to vec2 but we'll keep it in it's own structure in case I expand definition. Starting pos. defined.
float Angle = M_PI; // starting angle for player.



					// WALL DEFS & RESOURCES

SDL_Color CeilingColor = { 64,64,64,255 };
SDL_Color FloorColor = { 64,64,64,255 };

std::vector<WallLine> AllWalls;
SDL_Surface* WallTextureSurface;


// LIGHTING DEFS

float LightSize = 50.0;
Vector2 LightPos = { 1, 25 };
float MaxLightDistance = 100;
float LightFalloff = 1; // Light intensity diminishes by a value of 1 per pixel.
bool bReverseDirection = false;
int LightingUpdateThreshold = 0;
int CurrentLerpInterval = 0;
int TotalLerpInterval = 5000;




SDL_Point Offset; // used for setting an 'offset' for each view.

// VIEW DEFS x, y, w, h
SDL_Rect AbsoluteView = { 16, 16, ViewWidth, ViewHeight };
SDL_Rect TransformedView = { WindowWidth - ViewWidth - 16, 16, ViewWidth, ViewHeight };
SDL_Rect PerspectiveView = { 0, 0, WindowWidth, WindowHeight };

// DEBUG: Following is used for debug text/message only:
char message[1000] = "";
int InfoTextureWidth;
int InfoTextureHeight;
bool DrawingLastWall = false;
int WallNo; // Used to track which wall we are processing.  Debugging only.
int WallLightingIndexMin = 0, WallLightingIndexMax = 0;
float d_TotalWallWidth = 0;
float WallStep = 0;
float debug1 = 0.0f, debug2 = 0.0f, debug3 = 0.0f; //generic debug vars


bool LoadResources() // Map, Textures, sounds etc.
{

	SDL_Surface* BMPSurface = SDL_LoadBMP("brick64x64_24b.bmp");
	assert(BMPSurface != NULL); // Assert if BMP load failed.

	// Used for debugging surface pixel format.
	// Uint8* BMPPixelData = (Uint8*)BMPSurface->pixels; // get raw pixel data in weird format.
	// const char* surfacePixelFormatName = SDL_GetPixelFormatName(WallTextureSurface->format->format); 

	WallTextureSurface = SDL_ConvertSurfaceFormat(BMPSurface, SDL_PIXELFORMAT_RGB24, 0);

	// Further code used for debugging / PoC
	//SDL_Texture* WallTexture = SDL_CreateTextureFromSurface(g_renderer, WallTextureSurface);
	//SDL_Rect WallTextureRect = { 500, 500, WallTextureSurface->w, WallTextureSurface->h };
	//SDL_RenderCopy(g_renderer, WallTexture, NULL, &WallTextureRect);

	SDL_FreeSurface(BMPSurface);


	// Wall points must be defined in a clockwise 'winding order'
	AllWalls = {
	{ 0.0, 0.0, 50.0, 0.0,{ 255, 0, 255, 255 } },		// Wall 1
	{ 50.0, 0.0, 100.0, 25.0,{ 255, 0, 255, 255 } },	// Wall 2
	{ 100.0, 25.0, 50.0, 50.0,{ 0, 255, 255, 255 } },	// Wall 3
	{ 50.0, 50.0, 0.0, 50.0,{ 255, 0, 255, 255 } },		// Wall 4
	{ 0.0, 50.0, 0.0, 0.0,{ 255, 0, 255, 255 } }		// Wall 5
	};

	return true;
}


SDL_Color GetPixelFromTexture(SDL_Surface *Surface, int x, int y) // Get pixel color from given surface & xy coords.
{
	SDL_Color ReturnColor;

	SDL_LockSurface(Surface); // Lock surface so we can access raw pixel data.

	Uint8* PixelData = (Uint8*)Surface->pixels;

	int PixelRow = Surface->pitch * y;
	int PixelColumn = Surface->format->BytesPerPixel * x;
	int PixelIndexR = PixelRow + PixelColumn;
	int PixelIndexG = PixelRow + PixelColumn + 1;
	int PixelIndexB = PixelRow + PixelColumn + 2;


	ReturnColor.r = PixelData[PixelIndexR];
	ReturnColor.g = PixelData[PixelIndexG];
	ReturnColor.b = PixelData[PixelIndexB];
	ReturnColor.a = 255;

	SDL_UnlockSurface(Surface);

	return ReturnColor;

}



void HandleInput()
{

	while (SDL_PollEvent(&event) != 0)
		SDL_PumpEvents();

	Player CurrentPos = player; // Store current, valid player position in case new position is invalid / outside of sector.


	// Keyboard input
	const Uint8 *keystate = SDL_GetKeyboardState(NULL);

	if (keystate[SDL_SCANCODE_RIGHT])
		Angle -= 0.1;

	if (keystate[SDL_SCANCODE_LEFT])
		Angle += 0.1;

	if (keystate[SDL_SCANCODE_UP])
	{
		player.x += cos(Angle);
		player.y -= sin(Angle);
		if (IsPlayerCollidingWithWall())
		{
			player = CurrentPos;
		}
	}
	if (keystate[SDL_SCANCODE_DOWN])
	{
		player.x -= cos(Angle);
		player.y += sin(Angle);
		if (IsPlayerCollidingWithWall())
		{
			player = CurrentPos;
		}
	}
	if (keystate[SDL_SCANCODE_W])
	{
		player.x += cos(Angle);
		player.y -= sin(Angle);
		if (IsPlayerCollidingWithWall())
		{
			player = CurrentPos;
		}
	}
	if (keystate[SDL_SCANCODE_S])
	{
		player.x -= cos(Angle);
		player.y += sin(Angle);
		if (IsPlayerCollidingWithWall())
		{
			player = CurrentPos;
		}
	}
	if (keystate[SDL_SCANCODE_A])
	{
		player.x -= sin(Angle);
		player.y -= cos(Angle);
		if (IsPlayerCollidingWithWall())
		{
			player = CurrentPos;
		}
	}
	if (keystate[SDL_SCANCODE_D])
	{
		player.x += sin(Angle);
		player.y += cos(Angle);
		if (IsPlayerCollidingWithWall())
		{
			player = CurrentPos;
		}
	}
	if (keystate[SDL_SCANCODE_R])
	{
		player.x = 25;
		player.y = 25;
		Angle = 0.000000f;
		LightPos.x = 5; LightPos.y = 25;
	}
	if (keystate[SDL_SCANCODE_T])
		Angle += 45.0 / (180 / M_PI);
	if (keystate[SDL_SCANCODE_E])
		Angle += 1.0 / (180 / M_PI);
	if (keystate[SDL_SCANCODE_Y])
		Angle += 0.000001 / (180 / M_PI);
	if (keystate[SDL_SCANCODE_K])
	{
		if (LightPos.x > 0 && LightPos.x <= 100)
			LightPos.x += 1;
		else if (LightPos.x > 100)
			LightPos.x = 1;
	}
	if (keystate[SDL_SCANCODE_L])
	{
		if (LightPos.y > 0 && LightPos.y <= 50)
			LightPos.y += 1;
		else if (LightPos.y > 50)
			LightPos.y = 1;
	}
	if (keystate[SDL_SCANCODE_LCTRL])
		PlayerHeight = CrouchingHeight;
	if (!keystate[SDL_SCANCODE_LCTRL])
		PlayerHeight = StandingHeight;
	if (keystate[SDL_SCANCODE_Q])
	{
		ContinueGame = false;
	}


}

void MoveLight()
{
	LightingUpdateThreshold += TickDelta;
	CurrentLerpInterval += TickDelta;

	if (LightingUpdateThreshold > 100)
	{
		LightingUpdateThreshold = 0;

		float t = (float)CurrentLerpInterval / (float)TotalLerpInterval;

		if (!bReverseDirection)
		{
			LightPos = Lerp(Vector2(1, 25), Vector2(99, 25), t);
		}
		else
		{
			LightPos = Lerp(Vector2(99, 25), Vector2(1, 25), t);
		}

		if (CurrentLerpInterval > TotalLerpInterval)
		{
			bReverseDirection = !bReverseDirection;
			CurrentLerpInterval = 0;
		}
	}




}


void CountFPSAndLimit()
{
	// Calculate FPS

	PreviousTickCount = TickCount;
	TickCount = SDL_GetTicks();
	TickDelta = TickCount - PreviousTickCount;

	int DelayTicks = 0;

	if (TickDelta <= 16) // Limit FPS to about 60fps.
	{
		DelayTicks = 16 - TickDelta;
		SDL_Delay(DelayTicks);
	}

	fps = 1000 / (TickDelta + DelayTicks);


}




void DrawViews()
{

	// *** DRAW VIEWS **


	// ** DRAW AbsoluteView VIEWPORT **

	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(g_renderer, &AbsoluteView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(g_renderer, &AbsoluteView);


	// ** DRAW TRANSFORMED VIEWPORT **

	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(g_renderer, &TransformedView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(g_renderer, &TransformedView);

	Offset.x = TransformedView.x;
	Offset.y = TransformedView.y;

	// Draw player line
	SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(HalfViewWidth, HalfViewHeight, HalfViewWidth, HalfViewWidth - 5, Offset);

	// Change render colour to a light gray for intersect lines
	SDL_SetRenderDrawColor(g_renderer, 128, 128, 128, 128);

	// Draw intersect line
	DrawLineWithOffset(HalfViewWidth + -0.0001, HalfViewHeight - 0.0001, HalfViewWidth + -60, HalfViewHeight - 2, Offset);
	DrawLineWithOffset(HalfViewWidth + 0.0001, HalfViewHeight - 0.0001, HalfViewWidth + 60, HalfViewHeight - 2, Offset);

	// Change render colour to a light blue for viewplane trim
	SDL_SetRenderDrawColor(g_renderer, 128, 128, 256, 128);

	// Draw intersect line
	DrawLineWithOffset(HalfViewWidth + -0.0001, HalfViewHeight - 0.0001, HalfViewWidth + -(HalfViewWidth / 2), HalfViewHeight - HalfViewHeight, Offset);
	DrawLineWithOffset(HalfViewWidth + 0.0001, HalfViewHeight - 0.0001, HalfViewWidth + (HalfViewWidth / 2), HalfViewHeight - HalfViewHeight, Offset);

}


void DrawDebugText()
{
	// Draw info text
	sprintf(message,
		"FPS: %d \n"
		"Player X is %.2f, Player Y is %.2f. \n"
		"Angle is %.2f degrees or %.2f rads. Cosine (x) is %.2f. Sine (y) is %.2f. \n\n"
		" Debug1: %.2f Debug2: %.2f Debug3: L1: %d L2: %d R1: %d R2: %d \n\n"
		"Move with arrow keys / ADSW, r to reset position, e to turn 1 degree, t to turn 45 degrees, k & l to move the light (disabled for now), left ctrl to crouch\nPress q to quit.",
		fps, 
		player.x, player.y, //Player position
		fmod(Angle, 6.28) * 180 / 3.1415926, Angle, cos(Angle), sin(Angle),
		debug1, debug2, P1OnLeftSideOfClipPlane, P2OnLeftSideOfClipPlane, P1OnRightSideOfClipPlane, P2OnRightSideOfClipPlane
	);

	// Create surfaces, texture & rect needed for text rendering
	SDL_Surface* InfoTextSurface = TTF_RenderText_Blended_Wrapped(font, message, colorRed, WindowWidth - 15);
	SDL_Texture* InfoTexture = SDL_CreateTextureFromSurface(g_renderer, InfoTextSurface);
	InfoTextureWidth = InfoTextSurface->w;
	InfoTextureHeight = InfoTextSurface->h;

	SDL_Rect TextRenderQuad = { 15, ViewHeight + 30, InfoTextureWidth, InfoTextureHeight };
	SDL_RenderCopy(g_renderer, InfoTexture, NULL, &TextRenderQuad);

	// destroy data used to draw text
	SDL_FreeSurface(InfoTextSurface);
	SDL_DestroyTexture(InfoTexture);

	DrawingLastWall = false;
}


void RenderDebug(WallLine wallLine)
{

	Vector2 AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line.
	Vector2 AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line.

	Vector3 TransformedLineP1 = { 0.0, 0.0, 0.0 };  // used for drawing the current line in rotated view.
	Vector3 TransformedLineP2 = { 0.0, 0.0, 0.0 };  // used for drawing the current line in rotated view.

	Vector2 IntersectPoint1 = { 0.0, 0.0 };  
	Vector2 IntersectPoint2 = { 0.0, 0.0 };

	AbsoluteLineP1.x = wallLine.PT1x;
	AbsoluteLineP1.y = wallLine.PT1y;
	AbsoluteLineP2.x = wallLine.PT2x;
	AbsoluteLineP2.y = wallLine.PT2y;

	// LIGHTING CALCULATIONS (2D / Height not taken into account)


	// Calculate light position based on player's position.  Doesn't really need to happen for every wall draw.  Only needs to happen each frame.  Also doesn't need to be transformed as these values could be calculated using 'absolute' values.
	Vector2 TransformedLightPos;

	TransformedLightPos.x = (player.y - LightPos.y) * cos(Angle) - (LightPos.x - player.x) * sin(Angle);
	TransformedLightPos.y = (player.y - LightPos.y) * sin(Angle) + (LightPos.x - player.x) * cos(Angle);

	Vector2 MiddleOfWall;

	// This code depends on the clockwise winding order of the wall points.  Consider a counter clockwise case?
	float WallXDistance = AbsoluteLineP2.x - AbsoluteLineP1.x;
	float WallYDistance = AbsoluteLineP2.y - AbsoluteLineP1.y;

	float HalfOfWallXDistance = WallXDistance / 2;
	float HalfOfWallYDistance = WallYDistance / 2;

	MiddleOfWall.x = AbsoluteLineP2.x - HalfOfWallXDistance;
	MiddleOfWall.y = AbsoluteLineP2.y - HalfOfWallYDistance;

	Vector2 VectorToLight = { LightPos.x - MiddleOfWall.x, LightPos.y - MiddleOfWall.y };
	float LightDistance = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

	Vector2 VectorToLightNormalised = { VectorToLight.x / LightDistance, VectorToLight.y / LightDistance };

	Vector3 WallNormal = Cross(
		Vector3(HalfOfWallXDistance, HalfOfWallYDistance, 0),
		Vector3(0, 0, -1)
	);

	WallNormal.Normalise();

	float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLightNormalised);

	if (DrawingLastWall == true)
	{
		DrawDebugText(); // Debug text only applies to one wall
	}

	// ************* DRAW DEBUG VIEWPORTS *************

	//ABSOLUTE VIEW

	Offset.x = AbsoluteView.x;
	Offset.y = AbsoluteView.y;

	// Change render colour to Green.
	SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);

	// Draw Wall / Line in 'Absolute View'
	DrawLineWithOffset(AbsoluteLineP1.x, AbsoluteLineP1.y, AbsoluteLineP2.x, AbsoluteLineP2.y, Offset);

	// Draw player line with red
	SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(player.x, player.y, player.x + cos(Angle) * 5, player.y - sin(Angle) * 5, Offset);

	// Change render colour to white for drawing light position
	SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
	DrawPixelWithOffset(LightPos.x, LightPos.y, Offset);


	// Draw line from wall to light
	SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
	SDL_RenderDrawLine(g_renderer,
		Offset.x + MiddleOfWall.x,
		Offset.y + MiddleOfWall.y,
		Offset.x + MiddleOfWall.x + (VectorToLightNormalised.x * 5),
		Offset.y + MiddleOfWall.y + (VectorToLightNormalised.y * 5)
	);


	// Draw wall normal
	SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
	SDL_RenderDrawLine(g_renderer,
		Offset.x + MiddleOfWall.x,
		Offset.y + MiddleOfWall.y,
		Offset.x + MiddleOfWall.x + (WallNormal.x * 5),
		Offset.y + MiddleOfWall.y + (WallNormal.y * 5)
	);


	// TRANSFORMED VIEW

	Offset.x = TransformedView.x;
	Offset.y = TransformedView.y;


	// set line to be relative to player.
	TransformedLineP1.x = AbsoluteLineP1.x - player.x;
	TransformedLineP1.y = player.y - AbsoluteLineP1.y;

	TransformedLineP2.x = AbsoluteLineP2.x - player.x;
	TransformedLineP2.y = player.y - AbsoluteLineP2.y;

	// calculate depth (z val) of verticies based on where the player is looking
	TransformedLineP1.z = TransformedLineP1.y * sin(Angle) + TransformedLineP1.x * cos(Angle);
	TransformedLineP2.z = TransformedLineP2.y * sin(Angle) + TransformedLineP2.x * cos(Angle);

	// calculate x position of verticies based on where the player is looking
	TransformedLineP1.x = TransformedLineP1.y * cos(Angle) - TransformedLineP1.x * sin(Angle);
	TransformedLineP2.x = TransformedLineP2.y * cos(Angle) - TransformedLineP2.x * sin(Angle);
	
	/*
	IntersectPoint1 = Intersect(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, -0.0001, 0.0001, -60, 2);
	IntersectPoint2 = Intersect(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, 0.0001, 0.0001, 60, 2);

	// If the line is partially behind the player (crosses the viewplane, clip it)

	if (TransformedLineP1.z <= 0.0) // If PT1 is behind the player
	{

		if (IntersectPoint1.y > 0.0)
		{
			TransformedLineP1.x = IntersectPoint1.x;
			TransformedLineP1.z = IntersectPoint1.y;
		}
		else
		{
			TransformedLineP1.x = IntersectPoint2.x;
			TransformedLineP1.z = IntersectPoint2.y;
		}
	}

	if (TransformedLineP2.z <= 0.0) // If PT2 is behind the player
	{

		if (IntersectPoint1.y > 0.0)
		{
			TransformedLineP2.x = IntersectPoint1.x;
			TransformedLineP2.z = IntersectPoint1.y;
		}
		else
		{
			TransformedLineP2.x = IntersectPoint2.x;
			TransformedLineP2.z = IntersectPoint2.y;
		}
	} */

	
	
	Vector2 RightSideClipLineP1 = { (float)-0.0001, (float)0.0001 };
	Vector2 RightSideClipLineP2 = { -(HalfViewWidth / 2), (float)HalfViewHeight };

	Vector2 LeftSideClipLineP1 = { (float)0.0001, (float)0.0001 };
	Vector2 LeftSideClipLineP2 = { (HalfViewWidth / 2), (float)HalfViewHeight };

	P1OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
	P2OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));
	P1OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
	P2OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));

	bool LineBehindPlayer = false;
	bool LineOutOfFOV = false; // Line can be in FOV but behind the player due to the way 'LineSide' works so account for this with above bool 'LineBehindPlayer'.

	if (TransformedLineP1.z < 0.0 && TransformedLineP2.z < 0.0)
		LineBehindPlayer = true;

	if (!P1OnLeftSideOfClipPlane || !P2OnLeftSideOfClipPlane || !P1OnRightSideOfClipPlane || !P2OnLeftSideOfClipPlane)
		LineOutOfFOV = true;


	IntersectPoint1 = IntersectLineSegs(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, -0.0001, 0.0001, -(HalfViewWidth / 2), HalfViewHeight);
	IntersectPoint2 = IntersectLineSegs(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, 0.0001, 0.0001, HalfViewWidth / 2, HalfViewHeight);

	if (IntersectPoint1.IsZeroVector() && IntersectPoint2.IsZeroVector() && (LineBehindPlayer || LineOutOfFOV)) // If line is not in the player's FOV, skip drawing the line.
		return;


	if (IntersectPoint1.y > 0.0f) // If the line crossed the player's FOV on the right side
	{

			TransformedLineP2.x = IntersectPoint1.x;
			TransformedLineP2.z = IntersectPoint1.y;

	}

	if (IntersectPoint2.y > 0.0f) // If the line crossed the player's FOV on the left side
	{
			TransformedLineP1.x = IntersectPoint2.x;
			TransformedLineP1.z = IntersectPoint2.y;

	}

	float TransformedLineLength = sqrt((TransformedLineP2.x - TransformedLineP1.x) * (TransformedLineP2.x - TransformedLineP1.x) + (TransformedLineP2.z - TransformedLineP1.z) * (TransformedLineP2.z - TransformedLineP1.z));


	// Change render colour
	SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);

	// Draw wall / line.
	DrawLineWithOffset((HalfViewWidth - TransformedLineP1.x), (HalfViewHeight - TransformedLineP1.z), (HalfViewWidth - TransformedLineP2.x), (HalfViewHeight - TransformedLineP2.z), Offset);

	DrawPixelWithOffset(
		(float)HalfViewWidth - TransformedLightPos.x,
		(float)HalfViewHeight - TransformedLightPos.y,
		Offset);


}



void RenderWall(WallLine wallLine)
{

	Vector2 AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line.
	Vector2 AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line.

	Vector3 TransformedLineP1 = { 0.0, 0.0 };  // used for drawing the current line.
	Vector3 TransformedLineP2 = { 0.0, 0.0 };  // used for drawing the current line.

	Vector2 IntersectPoint1 = { 0.0, 0.0 };
	Vector2 IntersectPoint2 = { 0.0, 0.0 };

	WallData Wall = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; // Data used to draw the wall.  This structure holds the values of the '3d wall' projected on to the 2d 'camera'.


	AbsoluteLineP1.x = wallLine.PT1x; // Start by assigning the line points of the current 'wall' to AbsoluteLine
	AbsoluteLineP1.y = wallLine.PT1y;
	AbsoluteLineP2.x = wallLine.PT2x;
	AbsoluteLineP2.y = wallLine.PT2y;

	float AbsoluteLineLength = sqrt((AbsoluteLineP2.x - AbsoluteLineP1.x) * (AbsoluteLineP2.x - AbsoluteLineP1.x) + (AbsoluteLineP2.y - AbsoluteLineP1.y) * (AbsoluteLineP2.y - AbsoluteLineP1.y));


	// set line to be relative to player.
	TransformedLineP1.x = AbsoluteLineP1.x - player.x;
	TransformedLineP1.y = player.y - AbsoluteLineP1.y;

	TransformedLineP2.x = AbsoluteLineP2.x - player.x;
	TransformedLineP2.y = player.y - AbsoluteLineP2.y;

	// calculate depth of verticies based on where the player is looking
	TransformedLineP1.z = TransformedLineP1.y * sin(Angle) + TransformedLineP1.x * cos(Angle);
	TransformedLineP2.z = TransformedLineP2.y * sin(Angle) + TransformedLineP2.x * cos(Angle);

	// calculate x position of verticies based on where the player is looking
	TransformedLineP1.x = TransformedLineP1.y * cos(Angle) - TransformedLineP1.x * sin(Angle);
	TransformedLineP2.x = TransformedLineP2.y * cos(Angle) - TransformedLineP2.x * sin(Angle);





	// PERSPECTIVE VIEW / PROJECTION

	Offset.x = PerspectiveView.x;
	Offset.y = PerspectiveView.y;


	if (TransformedLineP1.z > 0.0 || TransformedLineP2.z > 0.0) // If either point of the wall is in front of the player (Note: not neccessarily in the FOV), draw the wall.
	{

		// Clip lines again but this time to the width of the FOV

		Vector2 RightSideClipLineP1 = { (float)-0.0001, (float)0.0001 };
		Vector2 RightSideClipLineP2 = { -(HalfViewWidth / 2), (float)HalfViewHeight };

		Vector2 LeftSideClipLineP1 = { (float)0.0001, (float)0.0001 };
		Vector2 LeftSideClipLineP2 = { (HalfViewWidth / 2), (float)HalfViewHeight };

		P1OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
		P2OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));
		P1OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
		P2OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));

		bool LineBehindPlayer = false;
		bool LineOutOfFOV = false; // Line can be in FOV but behind the player due to the way 'LineSide' works so account for this with above bool 'LineBehindPlayer'.

		if (TransformedLineP1.z < 0.0 && TransformedLineP2.z < 0.0)
			LineBehindPlayer = true;

		if (!P1OnLeftSideOfClipPlane || !P2OnLeftSideOfClipPlane || !P1OnRightSideOfClipPlane || !P2OnLeftSideOfClipPlane)
			LineOutOfFOV = true;


		IntersectPoint1 = IntersectLineSegs(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, -0.0001, 0.0001, -(HalfViewWidth / 2), HalfViewHeight);
		IntersectPoint2 = IntersectLineSegs(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, 0.0001, 0.0001, HalfViewWidth / 2, HalfViewHeight);

		if (IntersectPoint1.IsZeroVector() && IntersectPoint2.IsZeroVector() && (LineBehindPlayer || LineOutOfFOV)) // If line is not in the player's FOV, skip drawing the line.
			return;


		if (IntersectPoint1.y > 0.0f) // If the line crossed the player's FOV on the right side
		{

			TransformedLineP2.x = IntersectPoint1.x;
			TransformedLineP2.z = IntersectPoint1.y;

		}

		if (IntersectPoint2.y > 0.0f) // If the line crossed the player's FOV on the left side
		{
			TransformedLineP1.x = IntersectPoint2.x;
			TransformedLineP1.z = IntersectPoint2.y;

		}

		float TransformedLineLength = sqrt((TransformedLineP2.x - TransformedLineP1.x) * (TransformedLineP2.x - TransformedLineP1.x) + (TransformedLineP2.z - TransformedLineP1.z) * (TransformedLineP2.z - TransformedLineP1.z));


		// *** PERSPECTIVE DIVIDE *** What are FOV values? HFOV = 53.13 degrees according to calcs. VFOV = ?

		// x and y scalars are set depending on window height and width

		float xscale = PerspectiveView.w;
		float yscale = PerspectiveView.h * 8;

		if (PlayerHeight == CrouchingHeight)
		{
			yscale = yscale + (StandingHeight - CrouchingHeight);
		}

		Wall.x1 = -(TransformedLineP1.x * xscale) / TransformedLineP1.z; // Perspective divides to get verticies co-ords of wall to draw.

		Wall.y1a = -yscale / TransformedLineP1.z; // negative value for yscale is used as y coords on screen decrement as we get closer to the top of the window
		Wall.y1b = PlayerHeight / TransformedLineP1.z;

		Wall.x2 = -(TransformedLineP2.x * xscale) / TransformedLineP2.z;

		Wall.y2a = -yscale / TransformedLineP2.z;
		Wall.y2b = PlayerHeight / TransformedLineP2.z;




		// LIGHTING CALCULATIONS (2D / Height not taken into account)


		float LeftMostWallPreClip = min(Wall.x1, Wall.x2);
		float RightMostWallPreClip = max(Wall.x1, Wall.x2);

		float WallWidthPreClip = RightMostWallPreClip - LeftMostWallPreClip;

		float AmountOfWallLeftClipped = 0.0, AmountOfWallRightClipped = 0.0; // Used to lighting calcs.  Lighting calcs are performed on the entire wall but often only a portion of the wall is shown so we need to know how much of wall was clipped.


		
		// Clip walls with left & right side of view pane
		
		if (Wall.x1 <= -HalfWindowWidth + 1) // clip wall pt1 left
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, -HalfWindowWidth + 1, -HalfWindowHeight, -HalfWindowWidth + 1, HalfWindowHeight);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, -HalfWindowWidth + 1, -HalfWindowHeight, -HalfWindowWidth + 1, HalfWindowHeight);
			AmountOfWallLeftClipped = Wall.x1 - PerspectiveViewClipPosTop.x;
			Wall.x1 = PerspectiveViewClipPosTop.x;
			Wall.y1a = PerspectiveViewClipPosTop.y;
			Wall.y1b = PerspectiveViewClipPosBottom.y;
		}

		if (Wall.x2 <= -HalfWindowWidth + 1) // clip wall pt2 left
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, -HalfWindowWidth + 1, -HalfWindowHeight, -HalfWindowWidth + 1, HalfWindowHeight);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, -HalfWindowWidth + 1, -HalfWindowHeight, -HalfWindowWidth + 1, HalfWindowHeight);
			AmountOfWallLeftClipped = Wall.x2 - PerspectiveViewClipPosTop.x;
			Wall.x2 = PerspectiveViewClipPosTop.x;
			Wall.y2a = PerspectiveViewClipPosTop.y;
			Wall.y2b = PerspectiveViewClipPosBottom.y;
		}

		if (Wall.x1 >= HalfWindowWidth - 1) // clip wall pt1 right
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, HalfWindowWidth - 2, -HalfWindowHeight, HalfWindowWidth - 2, HalfWindowHeight);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, HalfWindowWidth - 2, -HalfWindowHeight, HalfWindowWidth - 2, HalfWindowHeight);
			AmountOfWallRightClipped = Wall.x1 - PerspectiveViewClipPosTop.x;
			Wall.x1 = PerspectiveViewClipPosTop.x;
			Wall.y1a = PerspectiveViewClipPosTop.y;
			Wall.y1b = PerspectiveViewClipPosBottom.y;
		}

		if (Wall.x2 >= HalfWindowWidth - 1) // clip wall pt2 right
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, HalfWindowWidth - 2, -HalfWindowHeight, HalfWindowWidth - 2, HalfWindowHeight);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, HalfWindowWidth - 2, -HalfWindowHeight, HalfWindowWidth - 2, HalfWindowHeight);
			AmountOfWallRightClipped = Wall.x2 - PerspectiveViewClipPosTop.x;
			Wall.x2 = PerspectiveViewClipPosTop.x;
			Wall.y2a = PerspectiveViewClipPosTop.y;
			Wall.y2b = PerspectiveViewClipPosBottom.y;
		}
		


		// Calculations for drawing the vertical lines of the wall, floor and ceiling.

		float YDeltaTop, YDeltaBottom; // Used to calculate/interpolate y positions for top & bottom of wall based on current x position of wall.

		float LeftMostWall = min(Wall.x1, Wall.x2);
		float RightMostWall = max(Wall.x1, Wall.x2);
		float HeightDeltaTop, HeightDeltaBottom;

		HeightDeltaTop = Wall.y2a - Wall.y1a; // Total height delta between end of wall and start of wall (top position)
		HeightDeltaBottom = Wall.y1b - Wall.y2b; // Total height delta between start of wall and end of wall (bottom position)

		float WallWidth = RightMostWall - LeftMostWall;


		float WallClipStart = abs(AmountOfWallLeftClipped) / abs(WallWidthPreClip);
		float WallClipEnd = (abs(AmountOfWallLeftClipped) + abs(WallWidth)) / abs(WallWidthPreClip);

		// This code depends on the clockwise winding order of the wall points.  Consider a counter clockwise case?
		float WallTotalXChange = AbsoluteLineP2.x - AbsoluteLineP1.x;
		float WallTotalYChange = AbsoluteLineP2.y - AbsoluteLineP1.y;

		float WallStartX = AbsoluteLineP1.x + (WallTotalXChange * WallClipStart);
		float WallStartY = AbsoluteLineP1.y + (WallTotalYChange * WallClipStart);

		float WallEndX = AbsoluteLineP1.x + (WallTotalXChange * WallClipEnd);
		float WallEndY = AbsoluteLineP1.y + (WallTotalYChange * WallClipEnd);

		float WallTotalVisibleXChange = WallEndX - WallStartX;
		float WallTotalVisibleYChange = WallEndY - WallStartY;

		Vector2 VectorToLight;
		float DistanceToLight;
		float LightScaler = 0.33;


		if (WallWidth > 1)
		{

			float StepXDelta = WallTotalVisibleXChange / WallWidth;
			float StepYDelta = WallTotalVisibleYChange / WallWidth;

			float NextLightScaler; // Used to hold the value of the LightScaler value 64 vlines across.  We'll interpolate towards this value.
			float LightScalerStep = 0.0;

			bool bFirstRun = true;

			for (int cl = LeftMostWall; cl <= RightMostWall; ++cl) // Loop over each x position of the wall. cl = current vertical line
			{
				if (cl % 64 == 0)
				{
					int NumPixelsToLookAhead = 64;

					if (cl + NumPixelsToLookAhead > RightMostWall)
					{
						NumPixelsToLookAhead = RightMostWall - cl;
					}

					float CurrentXWallPoint = WallStartX + (StepXDelta * ((cl + NumPixelsToLookAhead) - LeftMostWall));
					float CurrentYWallPoint = WallStartY + (StepYDelta * ((cl + NumPixelsToLookAhead) - LeftMostWall));

					VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
					DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

					// Normalise the VectorToLight
					VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

					Vector3 WallNormal = Cross( // What happens if one of these are 0?
						Vector3(StepXDelta * ((cl + NumPixelsToLookAhead) - LeftMostWall), StepYDelta * ((cl + NumPixelsToLookAhead) - LeftMostWall), 0),
						Vector3(0, 0, -1)
					);

					WallNormal.Normalise();

					float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLight);

					NextLightScaler = (1.00 / MaxLightDistance) * abs(DistanceToLight);
					NextLightScaler = WallToLightPerpendicularity - NextLightScaler;
					NextLightScaler = Clamp(NextLightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.

					LightScalerStep = (NextLightScaler - LightScaler) / NumPixelsToLookAhead;

				}
				else if (bFirstRun)
				{

					float CurrentXWallPoint = WallStartX + (StepXDelta * ((cl + 1) - LeftMostWall));
					float CurrentYWallPoint = WallStartY + (StepYDelta * ((cl + 1) - LeftMostWall));

					VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
					DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

					// Normalise the VectorToLight
					VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

					Vector3 WallNormal = Cross( // What happens if one of these are 0?
						Vector3(StepXDelta * ((cl + 1) - LeftMostWall), StepYDelta * ((cl + 1) - LeftMostWall), 0),
						Vector3(0, 0, -1)
					);

					WallNormal.Normalise();

					float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLight);

					LightScaler = (1.00 / MaxLightDistance) * abs(DistanceToLight);
					LightScaler = WallToLightPerpendicularity - LightScaler;
					LightScaler = Clamp(LightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.


					int Next64AlignedVLine = abs(cl % 64);
					// Calculate light vals for the next block of pixels
					CurrentXWallPoint = WallStartX + (StepXDelta * ((cl + Next64AlignedVLine) - LeftMostWall));
					CurrentYWallPoint = WallStartY + (StepYDelta * ((cl + Next64AlignedVLine) - LeftMostWall));

					VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
					DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

					// Normalise the VectorToLight
					VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

					WallNormal = Cross( // What happens if one of these are 0?
						Vector3(StepXDelta * ((cl + Next64AlignedVLine) - LeftMostWall), StepYDelta * ((cl + Next64AlignedVLine) - LeftMostWall), 0),
						Vector3(0, 0, -1)
					);

					WallNormal.Normalise();

					WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLight);

					NextLightScaler = (1.00 / MaxLightDistance) * abs(DistanceToLight);
					NextLightScaler = WallToLightPerpendicularity - NextLightScaler;
					NextLightScaler = Clamp(NextLightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.

					LightScalerStep = (NextLightScaler - LightScaler) / Next64AlignedVLine;


					bFirstRun = false;

				}
				else
				{
					LightScaler += LightScalerStep;
				}

				YDeltaTop = (cl - LeftMostWall) * (HeightDeltaTop / WallWidth); // Calculate the change in Y position for the current vertical line.  Used for the ceiling vertical line.
				YDeltaBottom = (cl - LeftMostWall) * (HeightDeltaBottom / WallWidth); // Calculate the change in Y position for the current vertical line.  Used for the floor vertical line.

				float WallDrawTop = Wall.y1a + YDeltaTop; // Calculate the Y position to draw the vertical ceiling line to.
				if (WallDrawTop < -HalfWindowHeight) // Set a minimum for this value to ensure vertical ceiling line at least starts at the top of the viewport and is not drawn above that (non-visible area)
					WallDrawTop = -HalfWindowHeight;

				float WallDrawBottom = Wall.y1b - YDeltaBottom; // Calculate the Y position to draw the vertical floor line from.
				if (WallDrawBottom > HalfWindowHeight) // Same as above but sets a maximum.
					WallDrawBottom = HalfWindowHeight;


				// COULD WE DRAW CEILING AND FLOOR AS TWO LARGE RECTANGLES AND JUST DRAW THE WALL ON TOP? LESS DRAW CALLS, MORE EFFICIENT?
				// Change render colour to the ceiling color.  Lines are drawn relative to the centre of the player's view.
				// SDL_SetRenderDrawColor(g_renderer, CeilingColor.r, CeilingColor.g, CeilingColor.b, CeilingColor.a);
				// DrawLineWithOffset(HalfWindowWidth  + cl, HalfWindowHeight + WallDrawTop, HalfWindowWidth  + cl, 1, Offset);

				// Change render colour to the floor color.  Lines are drawn relative to the centre of the player's view.
				// SDL_SetRenderDrawColor(g_renderer, FloorColor.r, FloorColor.g, FloorColor.b, FloorColor.a);
				// DrawLineWithOffset(HalfWindowWidth  + cl, HalfWindowHeight + WallDrawBottom, HalfWindowWidth  + cl, WindowHeight - 2, Offset);

				// get color to render to wall from wall texture.

				//int WallLineLength = 64 * sqrt((TransformedLineP2.x - TransformedLineP1.x) * (TransformedLineP2.x - TransformedLineP1.x) + (TransformedLineP2.y - TransformedLineP1.y) * (TransformedLineP2.y - TransformedLineP1.y)); // Get the full length of the wall.  This value is not dependant on the perspective divide / player POV.  We can use this to work out the exact position of the texture mapped wall co-ords.
				int WallLineLength = 16 * sqrt((AbsoluteLineP2.x - AbsoluteLineP1.x) * (AbsoluteLineP2.x - AbsoluteLineP1.x) + (AbsoluteLineP2.y - AbsoluteLineP1.y) * (AbsoluteLineP2.y - AbsoluteLineP1.y));
				int WallTextureSize = WallTextureSurface->w;
				// float WallLineTextureLength = WallLineLength - (WallLineLength % WallTextureSize); // Align to 64 pixels just to see how it looks
				float WallLineTextureLength = WallLineLength;

				float CurrentXValOfWall = cl - (int)LeftMostWall;

				int ScaledXValOfWallTextureStart = (int)(WallLineTextureLength * WallClipStart);
				int ScaledXValOfWallTextureStartMod = (int)(WallLineTextureLength * WallClipStart) % WallTextureSize;
				int ScaledXValOfWallTextureEnd = (int)(WallLineTextureLength * WallClipEnd);
				float ScaledXValOfWallTextureVisible = ScaledXValOfWallTextureEnd - ScaledXValOfWallTextureStart;

				if (DrawingLastWall)
				{
					DrawingLastWall = false;

					debug1 = ScaledXValOfWallTextureVisible;
					debug2 = WallWidth;
					debug3 = CurrentXValOfWall * (ScaledXValOfWallTextureVisible / WallWidth);

				}

				float CurrentScaledXValOfWall = CurrentXValOfWall * (ScaledXValOfWallTextureVisible / WallWidth);

				int TotalWallVlineHeight = WallDrawBottom - WallDrawTop;

				float CurrentXValOfWallTexture = (int)(CurrentScaledXValOfWall + ScaledXValOfWallTextureStartMod) % WallTextureSize; // Tile texture horizontally across the entire wall every time we get to the end of the WallTexture
				float VLineToTextureSizeRatio = (float)WallTextureSize / (float)TotalWallVlineHeight;

				/*
				for (int CurrentWallVlineYPosition = 0; CurrentWallVlineYPosition < TotalWallVlineHeight; CurrentWallVlineYPosition++)
				{

					SDL_Color CurrentPixelColor = GetPixelFromTexture(WallTextureSurface, CurrentXValOfWallTexture, (float)CurrentWallVlineYPosition * (float)VLineToTextureSizeRatio);
					SDL_SetRenderDrawColor(g_renderer, CurrentPixelColor.r * LightScaler, CurrentPixelColor.g * LightScaler, CurrentPixelColor.b * LightScaler, 255);
					SDL_RenderDrawPoint(g_renderer, HalfWindowWidth + cl, HalfWindowHeight + WallDrawTop + CurrentWallVlineYPosition);

					// Use gScreenSurface = SDL_GetWindowSurface( gWindow ) at some point instead of this slow method.

					//DrawLineWithOffset(HalfWindowWidth  + cl, HalfWindowHeight + WallDrawTop, HalfWindowWidth  + cl, HalfWindowHeight + WallDrawBottom, Offset);
				} */


				// Draw Wall
				// Change render colour to the wall color & apply lighting.  Lines are drawn relative to the centre of the player's view.

				SDL_SetRenderDrawColor(g_renderer, wallLine.wallColor.r * LightScaler, wallLine.wallColor.g * LightScaler, wallLine.wallColor.b * LightScaler, wallLine.wallColor.a);
				DrawLineWithOffset(HalfWindowWidth  + cl, HalfWindowHeight + WallDrawTop, HalfWindowWidth  + cl, HalfWindowHeight + WallDrawBottom, Offset);
			}


			// DRAW LIGHT SOURCE PIXEL.  This needs to be done per wall as each wall draws the ceiling above it & the light pixel could be above any of the walls.

			// Calculate light position.
			Vector2 TransformedLightPos;

			TransformedLightPos.x = (player.y - LightPos.y) * cos(Angle) - (LightPos.x - player.x) * sin(Angle);
			TransformedLightPos.y = (player.y - LightPos.y) * sin(Angle) + (LightPos.x - player.x) * cos(Angle);

			Vector2 LightPixel =
			{
				-(TransformedLightPos.x * PerspectiveView.w) / (TransformedLightPos.y),  // Perspective divides
				-yscale / TransformedLightPos.y
			};

			if (LightPixel.x > -HalfWindowWidth && LightPixel.x < HalfWindowWidth && LightPixel.y < 0 && LightPixel.y < HalfWindowHeight)
			{
				SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
				DrawPixelWithOffset(HalfWindowWidth + LightPixel.x, HalfWindowHeight + LightPixel.y, Offset);
			}







			// Change render colour to green
			SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);

			// Draw top line of wall
			DrawLineWithOffset(HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1a, HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2a, Offset);

			// Draw bottom line of wall
			DrawLineWithOffset(HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1b, HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2b, Offset);

			// Draw left line of wall
			DrawLineWithOffset(HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1a, HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1b, Offset);

			// Draw right line of wall
			DrawLineWithOffset(HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2a, HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2b, Offset);
		}

	}

}


void UnloadResources()
{
	SDL_FreeSurface(WallTextureSurface);
	TTF_Quit();
	SDL_Quit();
}


void MainLoop() // Primary game loop.  Structured in this way (separate function) so code can be compiled with emscripten easily.
{

	CountFPSAndLimit();

	HandleInput();



	// Set color to white & clear
	SDL_SetRenderDrawColor(g_renderer, 64, 64, 64, 255);
	SDL_RenderClear(g_renderer);

	MoveLight();

	// RenderWalls

	for (auto& wall : AllWalls)
	{
		if (&AllWalls.back() == &wall)
		{
			DrawingLastWall = true; // Used to determine when the last wall is being drawn so debug info can be drawn.  Switch to false after draw.
		}

		RenderWall(wall);
	}

	// Render debug viewports

	DrawViews();

	for (auto& wall : AllWalls)
	{
		if (&AllWalls.back() == &wall)
		{
			DrawingLastWall = true; // Used to determine when the last wall is being drawn so debug info can be drawn.  Switch to false after draw.
		}
		RenderDebug(wall);
	}



	// perform render
	SDL_RenderPresent(g_renderer);
}



int main(int argc, char * argv[])
{

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	TTF_Init();

	/* Create window and renderer for given surface */
	g_window = SDL_CreateWindow("Morgan's Renderer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WindowWidth, WindowHeight, 0);
	if (!g_window) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation fail : %s\n", SDL_GetError());
		ContinueGame = false;
	}
	g_surface = SDL_GetWindowSurface(g_window);
	g_renderer = SDL_CreateRenderer(g_window, -1, NULL);

	if (!g_renderer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render creation for surface fail : %s\n", SDL_GetError());
		ContinueGame = false;
	}


	/*
	if (SDL_CreateWindowAndRenderer(WindowWidth, WindowHeight, 0, &g_window, &g_renderer) < 0)
	{
	ContinueGame = false;
	} */



	font = TTF_OpenFont("font.ttf", 16);

	if (font == NULL || LoadResources() == false)
	{
		ContinueGame = false;
	}


#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(MainLoop, -1, 1);
#else
	while (ContinueGame) { MainLoop(); }
#endif


	return 0;
}
