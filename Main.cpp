#include <string>
#include <cmath>
#include <SDL.h>
#include <SDL_ttf.h>
#include "Utils.h"
// #include <cassert>



#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WindowWidth 1280
#define WindowHeight 720
#define HalfWindowHeight WindowHeight/2
#define HalfWindowWidth WindowWidth/2

#define ViewWidth 256
#define ViewHeight 256
#define HalfViewWidth ViewWidth / 2
#define HalfViewHeight ViewHeight / 2

/*
WARNING:  This code is messy & purely PoC.  You've been warned.

TO DO:
- Code clean up & break program into separate files.
- Make mobile friendly?

FEATURES TO IMPLEMENT:
- Optomisation - Draw rects where possible, remove divides (calc reciprocal once?), avoid squares,
- Draw Light box instead of a single pixel.
- Quadratic lighting falloff? (y = -x^2 + c)?
- Reading sectors from text file (duke3d/build format?)
- Wall Height.
- Jumping.
- BSP/Sectors/Portals?
- Better FOV understanding?

*/



// **** GLOBAL DATA ****


int TickCount = 0, PreviousTickCount, fps, TickDelta; // used for fps monitor
bool ContinueGame = true; // used as a conditional for exiting the app.



// PLAYER DEFS

const float CrouchingHeight = 6 * HalfWindowHeight; // Crouching Height
const float StandingHeight = 10 * HalfWindowHeight; // standing height.

float PlayerHeight = StandingHeight; // Start standing.
Player player = { 25.0, 25.0 }; // Could be moved to vec2 but we'll keep it in it's own structure in case I expand definition. Starting pos. defined.
float Angle = 0.00f; // starting angle for player.

SDL_Point virtualJoystickLocation = { 0, 0 };

// WALL DEFS & RESOURCES

const SDL_Color CeilingColor = { 96,96,128,255 };
const SDL_Color FloorColor = { 64,64,64,255 };

std::vector<WallLine> AllWalls;
SDL_Surface* WallTextureSurface;


// LIGHTING DEFS

// float LightSize = 50.0;
Vector2 LightPos = { 1, 25 };
float MaxLightDistance = 100;
// float LightFalloff = 1; // Light intensity diminishes by a value of 1 per pixel.
bool bReverseDirection = false;
int CurrentLerpInterval = 0;
const int TotalLerpInterval = 5000;


SDL_Point Offset; // used for setting an 'offset' for each view.

// VIEW DEFS x, y, w, h
const SDL_Rect AbsoluteView = { 16, 16, ViewWidth, ViewHeight };
const SDL_Rect TransformedView = { WindowWidth - ViewWidth - 16, 16, ViewWidth, ViewHeight };
const SDL_Rect PerspectiveView = { 0, 0, WindowWidth, WindowHeight };

// DEBUG: Following is used for debug text/message only:
char message[1000] = "";
bool DrawingLastWall = false;
int WallNo; // Used to track which wall we are processing.  Debugging only.
float debug1 = 0.0f, debug2 = 0.0f, debug3 = 0.0f, debug4 = 0.0f; //generic debug vars
// char* surfacePixelFormatName;
// char* surfacePixelFormatName2;



bool LoadResources() // Map, Textures, sounds etc.
{

	SDL_Surface* BMPSurface = SDL_LoadBMP("brick64x64_24b.bmp");
	// assert(BMPSurface != NULL); // Assert if BMP load failed.

	WallTextureSurface = SDL_ConvertSurfaceFormat(BMPSurface, SDL_PIXELFORMAT_RGB24, 0);

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


inline SDL_Color GetPixelFromTexture(SDL_Surface *Surface, int x, int y) // Get pixel color from given surface & xy coords.  Assumes unlocked surface.
{
	SDL_Color ReturnColor;

	Uint8* PixelData = (Uint8*)Surface->pixels;

	int PixelRow = (Surface->pitch * y);
	int PixelColumn = Surface->format->BytesPerPixel * x;
	int PixelIndexR = PixelRow + PixelColumn;
	int PixelIndexG = PixelRow + PixelColumn + 1;
	int PixelIndexB = PixelRow + PixelColumn + 2;

	ReturnColor.r = PixelData[PixelIndexR];
	ReturnColor.g = PixelData[PixelIndexG];
	ReturnColor.b = PixelData[PixelIndexB];

	return ReturnColor;

}



void HandleInput()
{
	Player CurrentPos = player; // Store current, valid player position in case new position is invalid / outside of sector.

	// ************* Touch input *************

	while (SDL_PollEvent(&event) != 0)
	{
		if (event.type == SDL_FINGERDOWN)
		{
			virtualJoystickLocation.x = event.tfinger.x * WindowWidth;
			virtualJoystickLocation.y = event.tfinger.y * WindowHeight;
		}
		else if (event.type == SDL_FINGERMOTION)
		{
			SDL_Point virtualJoystickDelta;
			virtualJoystickDelta.x = virtualJoystickLocation.x - event.tfinger.x * WindowWidth;
			virtualJoystickDelta.y = virtualJoystickLocation.y - event.tfinger.y * WindowHeight;

			if (abs(virtualJoystickDelta.x) > abs(virtualJoystickDelta.y))
			{
				if (virtualJoystickDelta.x < 100)
				{
					Angle -= 0.1;
				}
				if (virtualJoystickDelta.x > 100)
				{
					Angle += 0.1;
				}
			}
			else
			{
				if (virtualJoystickDelta.y > 100)
				{
					player.x += cos(Angle);
					player.y -= sin(Angle);
					if (IsPlayerCollidingWithWall())
					{
						player = CurrentPos;
					}
				}
				if (virtualJoystickDelta.y < 100)
				{
					player.x -= cos(Angle);
					player.y += sin(Angle);
					if (IsPlayerCollidingWithWall())
					{
						player = CurrentPos;
					}
				}
			}

		}
		SDL_PumpEvents();
	}

	// ************* Keyboard input ****************

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

	if (keystate[SDL_SCANCODE_LCTRL])
		PlayerHeight = CrouchingHeight;

	if (!keystate[SDL_SCANCODE_LCTRL])
		PlayerHeight = StandingHeight;

	if (keystate[SDL_SCANCODE_Q])
		ContinueGame = false;

}

void MoveLight()
{
	CurrentLerpInterval += TickDelta;


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


void CountFPSAndLimit()
{
	// Calculate FPS

	PreviousTickCount = TickCount;
	TickCount = SDL_GetTicks();
	TickDelta = TickCount - PreviousTickCount;

	int DelayTicks = 0;

	if (TickDelta <= 16) // Limit FPS to about 60 fps.
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

	// Change render colour to a light blue for viewplane trim
	SDL_SetRenderDrawColor(g_renderer, 128, 128, 255, 128);

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
		"Angle is %.2f degrees or %.2f rads. \n\n"
		//" Debug1: %.2f Debug2: %.2f Debug3:  %.2f  Debug4:  %.2f \n\n"
		//" debugstr1: %s debugstr2: %s\n\n"
		"Touch & drag anywhere on canvas to move \n"
		"Move with arrow keys / ADSW, r to reset position, e to turn 1 degree, t to turn 45 degrees, left ctrl to crouch\nPress q to quit.",
		fps,
		player.x, player.y, //Player position
		fmod(Angle, 6.28) * 180 / 3.1415926, Angle
		//debug1, debug2, debug3, debug4,
		// surfacePixelFormatName, surfacePixelFormatName2
	);

	// Create surfaces, texture & rect needed for text rendering
	SDL_Surface* InfoTextSurface = TTF_RenderText_Blended_Wrapped(font, message, colorRed, WindowWidth - 15);
	SDL_Texture* InfoTexture = SDL_CreateTextureFromSurface(g_renderer, InfoTextSurface);

	SDL_Rect TextRenderQuad = { 15, ViewHeight + 30, InfoTextSurface->w, InfoTextSurface->h };
	SDL_RenderCopy(g_renderer, InfoTexture, NULL, &TextRenderQuad);

	// destroy data used to draw text
	SDL_FreeSurface(InfoTextSurface);
	SDL_DestroyTexture(InfoTexture);

	DrawingLastWall = false;
}


void RenderDebug(WallLine wallLine) // Used to draw data in debug viewports as well as debug text.  Must be drawn after drawing the 'main' view.
{

	Vector2 AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line.
	Vector2 AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line.

	Vector3 TransformedLineP1 = { 0.0, 0.0, 0.0 };  // used for drawing the current line rotated around the player.
	Vector3 TransformedLineP2 = { 0.0, 0.0, 0.0 };  // used for drawing the current line rotated around the player

	Vector2 IntersectPoint1 = { 0.0, 0.0 };  // Used to trim walls to what is in the player's FOV.
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

	// This code depends on the clockwise winding order of the wall points.

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
	
	// Clip lines against the players FOV.


	Vector2 RightSideClipLineP1 = { (float)-0.0001, (float)0.0001 };
	Vector2 RightSideClipLineP2 = { -(HalfViewWidth / 2), (float)HalfViewHeight };

	Vector2 LeftSideClipLineP1 = { (float)0.0001, (float)0.0001 };
	Vector2 LeftSideClipLineP2 = { (HalfViewWidth / 2), (float)HalfViewHeight };

	bool P1OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
	bool P2OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));
	bool P1OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
	bool P2OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));

	bool LineBehindPlayer = false;
	bool LineOutOfFOV = false; // Line can be in FOV but behind the player due to the way 'LineSide' works so account for this with above bool 'LineBehindPlayer'.

	if (TransformedLineP1.z < 0.0 && TransformedLineP2.z < 0.0)
		LineBehindPlayer = true;

	if (!P1OnLeftSideOfClipPlane || !P2OnLeftSideOfClipPlane || !P1OnRightSideOfClipPlane || !P2OnRightSideOfClipPlane)
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

	// Change render colour to green
	SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);

	// Draw wall / line.
	DrawLineWithOffset((HalfViewWidth - TransformedLineP1.x), (HalfViewHeight - TransformedLineP1.z), (HalfViewWidth - TransformedLineP2.x), (HalfViewHeight - TransformedLineP2.z), Offset);

	DrawPixelWithOffset(
		(float)HalfViewWidth - TransformedLightPos.x,
		(float)HalfViewHeight - TransformedLightPos.y,
		Offset);

	if (DrawingLastWall == true)
	{
		DrawDebugText(); // Debug text only applies to one wall
	}

}



void RenderWall(WallLine wallLine)
{

	Vector2 AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line in 'absolute' view.
	Vector2 AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line in 'absolute' view.

	Vector3 TransformedLineP1 = { 0.0, 0.0 };  // used for drawing the current line rotated around the player.
	Vector3 TransformedLineP2 = { 0.0, 0.0 };  // used for drawing the current line rotated around the player.

	Vector2 IntersectPoint1 = { 0.0, 0.0 }; // used for point of inter
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

		// ********** FOV CLIPPING  ********** Clip lines against the player's FOV.  What are FOV values? HFOV = 53.13 degrees according to calcs. VFOV = ?

		const Vector2 RightSideClipLineP1 = { (float)-0.0001, (float)0.0001 };
		const Vector2 RightSideClipLineP2 = { -(HalfViewWidth / 2), (float)HalfViewHeight };

		const Vector2 LeftSideClipLineP1 = { (float)0.0001, (float)0.0001 };
		const Vector2 LeftSideClipLineP2 = { (HalfViewWidth / 2), (float)HalfViewHeight };

		bool P1OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
		bool P2OnLeftSideOfClipPlane = LineSide(LeftSideClipLineP1, LeftSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));
		bool P1OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP1.x, TransformedLineP1.z));
		bool P2OnRightSideOfClipPlane = !LineSide(RightSideClipLineP1, RightSideClipLineP2, Vector2(TransformedLineP2.x, TransformedLineP2.z));

		bool LineBehindPlayer = false;
		bool LineOutOfFOV = false; // Line can be in FOV but behind the player due to the way 'LineSide' works so account for this with above bool 'LineBehindPlayer'.

		if (TransformedLineP1.z < 0.0 && TransformedLineP2.z < 0.0)
			LineBehindPlayer = true;

		if (!P1OnLeftSideOfClipPlane || !P2OnLeftSideOfClipPlane || !P1OnRightSideOfClipPlane || !P2OnRightSideOfClipPlane)
			LineOutOfFOV = true;


		IntersectPoint1 = IntersectLineSegs(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, -0.0001, 0.0001, -(HalfViewWidth / 2), HalfViewHeight);
		IntersectPoint2 = IntersectLineSegs(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, 0.0001, 0.0001, HalfViewWidth / 2, HalfViewHeight);

		if (IntersectPoint1.IsZeroVector() && IntersectPoint2.IsZeroVector() && (LineBehindPlayer || LineOutOfFOV)) // If line is not in the player's FOV, skip drawing the line.
			return;

		float PercentageLeftLineTrim = 0.0;
		float PercentageRightLineTrim = 0.0;


		if (IntersectPoint1.y > 0.0f) // If the line crossed the player's FOV on the right side
		{
			float TrimLineLength = sqrt((TransformedLineP2.x - IntersectPoint1.x) * (TransformedLineP2.x - IntersectPoint1.x) + (TransformedLineP2.z - IntersectPoint1.y) * (TransformedLineP2.z - IntersectPoint1.y));
			PercentageRightLineTrim = TrimLineLength / AbsoluteLineLength;

			TransformedLineP2.x = IntersectPoint1.x;
			TransformedLineP2.z = IntersectPoint1.y;

		}

		if (IntersectPoint2.y > 0.0f) // If the line crossed the player's FOV on the left side
		{
			float TrimLineLength = sqrt((TransformedLineP1.x - IntersectPoint2.x) * (TransformedLineP1.x - IntersectPoint2.x) + (TransformedLineP1.z - IntersectPoint2.y) * (TransformedLineP1.z - IntersectPoint2.y));
			PercentageLeftLineTrim = TrimLineLength / AbsoluteLineLength;
			
			TransformedLineP1.x = IntersectPoint2.x;
			TransformedLineP1.z = IntersectPoint2.y;

		}

		const float TransformedLineLength = sqrt((TransformedLineP2.x - TransformedLineP1.x) * (TransformedLineP2.x - TransformedLineP1.x) + (TransformedLineP2.z - TransformedLineP1.z) * (TransformedLineP2.z - TransformedLineP1.z));
		const float WallInFOVRatio = TransformedLineLength / AbsoluteLineLength;



		// *** PERSPECTIVE DIVIDE *** 

		// x and y scalars are set depending on window height and width

		float xscale = PerspectiveView.w;
		float yscale = PerspectiveView.h * 8;

		if (PlayerHeight == CrouchingHeight) // If the player is crouching you need to draw the wall above the player's centre of view a little bit taller
		{
			yscale = yscale + (StandingHeight - CrouchingHeight);
		}

		// x1 = left X of wall, x2 = right X of wall, y1a = left Y of wall (top position), y1b = left Y of wall (bottom pos), y2a = right Y of wall (top position), y2b = right Y of wall (bottom position) 

		Wall.x1 = -(TransformedLineP1.x * xscale) / TransformedLineP1.z; // Perspective divides to get verticies co-ords of wall to draw.

		Wall.y1a = -yscale / TransformedLineP1.z; // negative value for yscale is used as y coords on screen decrement as we get closer to the top of the window
		Wall.y1b = PlayerHeight / TransformedLineP1.z; // PlayerHeight is used as this is how much wall you want to draw under the centre line of the screen giving the experience of height.

		Wall.x2 = -(TransformedLineP2.x * xscale) / TransformedLineP2.z;

		Wall.y2a = -yscale / TransformedLineP2.z;
		Wall.y2b = PlayerHeight / TransformedLineP2.z;




		// *** LIGHTING CALCULATIONS (2D / Height not taken into account) ****


		// Calculations for drawing the vertical lines of the wall, floor and ceiling.



		const float LeftMostWall = Wall.x1;  // Not really neccessary but makes the code more readable.
		const float RightMostWall = Wall.x2;

		const float HeightDeltaTop = Wall.y2a - Wall.y1a; // Total height delta between end of wall and start of wall (top position)
		const float HeightDeltaBottom = Wall.y1b - Wall.y2b; // Total height delta between start of wall and end of wall (bottom position)

		const float WallWidth = RightMostWall - LeftMostWall;


		// This code depends on the clockwise winding order of the wall points.
		const float WallTotalXChange = AbsoluteLineP2.x - AbsoluteLineP1.x;
		const float WallTotalYChange = AbsoluteLineP2.y - AbsoluteLineP1.y;

		const float WallEndX = AbsoluteLineP1.x + WallTotalXChange;
		const float WallEndY = AbsoluteLineP1.y + WallTotalYChange;

		Vector2 VectorToLight;
		float DistanceToLight;
		float LightScaler = 0.33f;

		float StepXDelta = WallTotalXChange / WallWidth;
		float StepYDelta = WallTotalYChange / WallWidth;

		float NextLightScaler; // Used to hold the value of the LightScaler value 64 vlines across.  We'll interpolate towards this value.
		float LightScalerStep = 0.0;

		bool bFirstRun = true; // When calculating the wall lighting, we have to handle the first column a bit differently than the others 
			
		// These only really need to be generated once per wall
		Uint8* SurfacePixels = (Uint8*) g_surface->pixels;
		Uint8* WallTexturePixels = (Uint8*)WallTextureSurface->pixels;
		const int WallLineTextureLength = 8 * AbsoluteLineLength; // Acquire full length of wall
		const int WallTextureSize = WallTextureSurface->w;
		const int CalculateLightingEveryXPixels = 64; // Used to make lighting more efficient.  Light calcs are actually only done every 64 pixels and interpolated/blended between.

		for (int CurrentVerticalLine = LeftMostWall; CurrentVerticalLine < RightMostWall ; ++CurrentVerticalLine) // Loop over each x position of the wall.
		{
			if (CurrentVerticalLine % CalculateLightingEveryXPixels == 0) // If it's time to recalculate the lighting values (once every X pixels)
			{
				int NumPixelsToLookAhead = CalculateLightingEveryXPixels;

				if (CurrentVerticalLine + NumPixelsToLookAhead > RightMostWall) // if we're near the end of the wall, don't calculate lighting for a position that doesn't exist.
				{
					NumPixelsToLookAhead = RightMostWall - CurrentVerticalLine;
				}

				const float CurrentXWallPoint = AbsoluteLineP1.x + (StepXDelta * ((CurrentVerticalLine + NumPixelsToLookAhead) - LeftMostWall));
				const float CurrentYWallPoint = AbsoluteLineP1.y + (StepYDelta * ((CurrentVerticalLine + NumPixelsToLookAhead) - LeftMostWall));

				VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
				DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

				// Normalise the VectorToLight
				VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

				Vector3 WallNormal = Cross( // What happens if one of these are 0?
					Vector3(StepXDelta * ((CurrentVerticalLine + NumPixelsToLookAhead) - LeftMostWall), StepYDelta * ((CurrentVerticalLine + NumPixelsToLookAhead) - LeftMostWall), 0),
					Vector3(0, 0, -1)
				);

				WallNormal.Normalise();

				const float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLight);

				NextLightScaler = (1.00 / MaxLightDistance) * abs(DistanceToLight);
				NextLightScaler = WallToLightPerpendicularity - NextLightScaler;
				NextLightScaler = Clamp(NextLightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.

				LightScalerStep = (NextLightScaler - LightScaler) / NumPixelsToLookAhead;

			}
			else if (bFirstRun) // If it's the first run, perform calculations for the current/first position as well as for the first time we re-calculate lighting with CalculateLightingEveryXPixels (probably set to 64)
			{

				float CurrentXWallPoint = AbsoluteLineP1.x + (StepXDelta * ((CurrentVerticalLine + 1) - LeftMostWall));
				float CurrentYWallPoint = AbsoluteLineP1.y + (StepYDelta * ((CurrentVerticalLine + 1) - LeftMostWall));

				VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
				DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

				// Normalise the VectorToLight
				VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

				Vector3 WallNormal = Cross( 
					Vector3(StepXDelta * ((CurrentVerticalLine + 1) - LeftMostWall), StepYDelta * ((CurrentVerticalLine + 1) - LeftMostWall), 0),
					Vector3(0, 0, -1)
				);

				WallNormal.Normalise();

				float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLight);

				LightScaler = (1.00 / MaxLightDistance) * abs(DistanceToLight);
				LightScaler = WallToLightPerpendicularity - LightScaler;
				LightScaler = Clamp(LightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.


				const int Next64AlignedVLine = abs(CurrentVerticalLine % CalculateLightingEveryXPixels);
				// Calculate light vals for the next block of pixels
				CurrentXWallPoint = AbsoluteLineP1.x + (StepXDelta * ((CurrentVerticalLine + Next64AlignedVLine) - LeftMostWall));
				CurrentYWallPoint = AbsoluteLineP1.y + (StepYDelta * ((CurrentVerticalLine + Next64AlignedVLine) - LeftMostWall));

				VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
				DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

				// Normalise the VectorToLight
				VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

				WallNormal = Cross( // What happens if one of these are 0?
					Vector3(StepXDelta * ((CurrentVerticalLine + Next64AlignedVLine) - LeftMostWall), StepYDelta * ((CurrentVerticalLine + Next64AlignedVLine) - LeftMostWall), 0),
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

			// *************** TEXTURE MAPPING & ACTUAL WALL DRAWING / PIXEL PUSHING *********************

			// Used to calculate/interpolate y positions for top & bottom of wall based on current x position of wall.
			const float YDeltaTop = (CurrentVerticalLine - LeftMostWall) * (HeightDeltaTop / WallWidth); // Calculate the change in Y position for the current vertical line.  Used for the ceiling vertical line.
			const float YDeltaBottom = (CurrentVerticalLine - LeftMostWall) * (HeightDeltaBottom / WallWidth); // Calculate the change in Y position for the current vertical line.  Used for the floor vertical line.

			float WallDrawTop = Wall.y1a + YDeltaTop; // Calculate the Y position to draw the vertical ceiling line to.
			float TrimmedTop = 0.0f;
			float TrimmedBottom = 0.0f;

			if (WallDrawTop < -HalfWindowHeight) // Set a minimum for this value to ensure vertical ceiling line starts at the top of the viewport and is not drawn above that (non-visible area). Save the amount we are are removing/trimming for later calcs regarding how much texture we have to draw.
			{
				TrimmedTop = -HalfWindowHeight - WallDrawTop;
				WallDrawTop = -HalfWindowHeight;
			}

			float WallDrawBottom = Wall.y1b - YDeltaBottom; // Calculate the Y position to draw the vertical floor line from.
				
			if (WallDrawBottom > HalfWindowHeight) // Similar to above but sets a maximum so we aren't drawing below the screen.
			{
				TrimmedBottom = WallDrawBottom - HalfWindowHeight;
				WallDrawBottom = HalfWindowHeight;
			}



			// Find X val code of texture to use
			int CurrentXValOfWall = CurrentVerticalLine - (int)LeftMostWall; // Current X coord of wall starting from 0 as opposed to some negative value
			const int CurrentXValOfWallStart =  (int)((float)WallLineTextureLength * PercentageLeftLineTrim) % WallTextureSize; // If the wall is being clipped by the left side of the screen, find the appropriate start X co-ord for the texture
			CurrentXValOfWall = CurrentXValOfWall * ((WallLineTextureLength * WallInFOVRatio) / WallWidth);  // Find the ratio of how much of the wall texture to use based on how much of the wall is in the FOV compared to the total width of the wall being drawn.
			const float CurrentXValOfWallTexture = (CurrentXValOfWall + CurrentXValOfWallStart) % WallTextureSize; // Find the X value to use to draw the wall in texture space.  Modulo so we tile texture horizontally across the entire wall every time we get to the end of the WallTexture

			// Find Y value code of texture to use
			const int TotalWallVlineHeight = WallDrawBottom - WallDrawTop; // Total height of the column being drawn.
			const float WallTextureVlineMinValue = (TrimmedTop / ( TotalWallVlineHeight + TrimmedTop + TrimmedBottom)) * WallTextureSize; // Used to set a minimum y value in texture space if the wall is getting clipped due to being above the FOV
			const float WallTextureVlineMaxValueRatio = ((TrimmedTop + TotalWallVlineHeight) / (TotalWallVlineHeight + TrimmedTop + TrimmedBottom)); // Used to find the ratio used to set a maximum y value in texture space if the wall is getting clipped due to being below the FOV
			float VLineToTextureSizeRatio = (float)WallTextureSize / (float)(TotalWallVlineHeight + TrimmedTop + TrimmedBottom); // Used to find the ratio between the Wall Texture Size & the total height of the wall.
			VLineToTextureSizeRatio *= WallTextureVlineMaxValueRatio; // Used to both limit the maximum value (in texture space) of the y co-ord of the wall being drawn in addition to squashing all 

			/*
			if (DrawingLastWall)
			{
				DrawingLastWall = false;

				//debug1 = WallTextureVlineMaxValue;
				//debug2 = 64 - (WallTextureVlineMinValue + WallTextureVlineMaxValue);
				//debug3 = WallTextureVlineMinValue + (float)0.0 * (float)VLineToTextureSizeRatio - WallTextureVlineMaxValue;
				//debug4 = WallTextureVlineMinValue + (float)TotalWallVlineHeight * (float)VLineToTextureSizeRatio - WallTextureVlineMaxValue;

			}*/

			int XPosOfPixel = Clamp(HalfWindowWidth + CurrentVerticalLine, 0, WindowWidth - 1) * 4; // Calculate position in memory of X position of pixel to draw & make sure we don't draw outside of the window surface.

			
			for (int CurrentWallVlineYPosition = 0; CurrentWallVlineYPosition < TotalWallVlineHeight; CurrentWallVlineYPosition++) // For every pixel we are about to draw vertically, Get pixel colour from wall texture.
			{
				// get color to render to wall from wall texture.

				const int TextPixelRow = (WallTextureSurface->pitch * (int)(WallTextureVlineMinValue + (float)CurrentWallVlineYPosition * (float)VLineToTextureSizeRatio));
				const int TextPixelColumn = WallTextureSurface->format->BytesPerPixel * CurrentXValOfWallTexture;
				const int TextPixelIndexR = TextPixelRow + TextPixelColumn;
				const int TextPixelIndexG = TextPixelRow + TextPixelColumn + 1;
				const int TextPixelIndexB = TextPixelRow + TextPixelColumn + 2;

				const Uint8 r = WallTexturePixels[TextPixelIndexR];
				const Uint8 g = WallTexturePixels[TextPixelIndexG];
				const Uint8 b = WallTexturePixels[TextPixelIndexB];
				
				const int YPosOfPixel = HalfWindowHeight + WallDrawTop + CurrentWallVlineYPosition; // Calculate Y position of pixel to push

				// Exact pixel location in memory
				const int PixelLocation = (YPosOfPixel * g_surface->pitch) + XPosOfPixel;
				
				// LightScaler = lighting for current line.

#ifdef __EMSCRIPTEN__
				SurfacePixels[PixelLocation] = r  * LightScaler;
				SurfacePixels[PixelLocation + 1] = g * LightScaler;
				SurfacePixels[PixelLocation + 2] =  b * LightScaler;
#else
				SurfacePixels[PixelLocation] = b * LightScaler;
				SurfacePixels[PixelLocation + 1] = g * LightScaler;
				SurfacePixels[PixelLocation + 2] = r  * LightScaler;
#endif
			} 
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

		if (LightPixel.x > -HalfWindowWidth && LightPixel.x < HalfWindowWidth && LightPixel.y < 0 && LightPixel.y < HalfWindowHeight) // Only draw the pixel if it's above a wall.
		{
			SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
			DrawPixelWithOffset(HalfWindowWidth + LightPixel.x, HalfWindowHeight + LightPixel.y, Offset);
		}


		// **** DRAW WALL WIREFRAME

		// Change render colour to green
		// SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);

		// Draw top line of wall
		// DrawLineWithOffset(HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1a, HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2a, Offset);

		// Draw bottom line of wall
		// DrawLineWithOffset(HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1b, HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2b, Offset);

		// Draw left line of wall
		// DrawLineWithOffset(HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1a, HalfWindowWidth + Wall.x1, HalfWindowHeight + Wall.y1b, Offset);

		// Draw right line of wall
		// DrawLineWithOffset(HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2a, HalfWindowWidth + Wall.x2, HalfWindowHeight + Wall.y2b, Offset);

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
	

	// Clear background to ceiling colour.
	SDL_SetRenderDrawColor(g_renderer, CeilingColor.r, CeilingColor.g, CeilingColor.b, 255);
	SDL_RenderClear(g_renderer);

	// Draw floor on bottom half of screen.
	SDL_SetRenderDrawColor(g_renderer, FloorColor.r, FloorColor.g, FloorColor.b, 255);
	SDL_Rect FloorRect = { 0, HalfWindowHeight, WindowWidth, WindowHeight };
	SDL_RenderFillRect(g_renderer, &FloorRect);

	MoveLight();

	// Render Walls
	SDL_LockSurface(WallTextureSurface); // Lock surface so we can access raw pixel data.
	SDL_LockSurface(g_surface);

	for (auto& wall : AllWalls)
	{
		if (&AllWalls.back() == &wall)
		{
			DrawingLastWall = true; // Used to determine when the last wall is being drawn so debug info can be drawn.  Switch to false after draw.
		}

		RenderWall(wall);
	}
	
	SDL_UnlockSurface(WallTextureSurface); // Unlock surface now that we are no longer directly accessing pixels.
	SDL_UnlockSurface(g_surface);
	
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

	// Flip the screen buffers
	SDL_RenderPresent(g_renderer);
}



int main(int argc, char * argv[])
{

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	TTF_Init();

	/* Create window for given surface */
	g_window = SDL_CreateWindow("Morgan's Renderer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WindowWidth, WindowHeight, 0);
	if (!g_window) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation fail : %s\n", SDL_GetError());
		ContinueGame = false;
	}

	g_surface = SDL_GetWindowSurface(g_window);

	g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE); //SDL_RENDERER_ACCELERATED // SDL_RENDERER_SOFTWARE
	if (!g_renderer) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render creation for surface fail : %s\n", SDL_GetError());
		ContinueGame = false;
	}


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
