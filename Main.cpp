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


/*
WARNING:  This code is messy & purely PoC.  You've been warned.

TO DO:
- Code clean up & break program into separate files.
- Better clipping.
- Make mobile friendly?

FEATURES TO IMPLEMENT:
- Optomisation
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



bool ContinueGame = true; // used as a conditional for exiting the app.

const int ViewWidth = 300;
const int ViewHeight = 300;


// PLAYER DEFS

int CrouchingHeight = 4 * (WindowHeight / 2); // Crouching Height
int StandingHeight = 10 * (WindowHeight / 2); // standing height.

int PlayerHeight = StandingHeight; // Start standing.
Player player = { 25, 25 }; // Could be moved to vec2 but we'll keep it in it's own structure in case I expand definition. Starting pos. defined.
double Angle = 1.5700000f; // starting angle for player.



// WALL DEFS

SDL_Color CeilingColor = { 64,64,64,255 };
SDL_Color FloorColor = { 192,192,192,255 };



// LIGHTING DEFS

float LightSize = 50.0;

Vector2 LightPos = { 10, 10 };
float MaxLightDistance = 150;
float LightFalloff = 1; // Light intensity diminishes by a value of 1 per pixel.


std::vector<WallLine> AllWalls;

SDL_Point Offset; // used for setting an 'offset' for each view.

// VIEW DEFS x, y, w, h
SDL_Rect AbsoluteView = { 15, 15, ViewWidth, ViewHeight };
SDL_Rect TransformedView = { WindowWidth - ViewWidth - 15, 15, ViewWidth, ViewHeight };
SDL_Rect PerspectiveView = { 0, 0, WindowWidth, WindowHeight};

// DEBUG: Following is used for debug text/message only:
char message[3000] = "";
int InfoTextureWidth;
int InfoTextureHeight;
bool DrawingLastWall = false;
int WallNo; // Used to track which wall we are processing.  Debugging only.
int WallLightingIndexMin = 0, WallLightingIndexMax = 0;
float d_TotalWallWidth = 0;
float WallStep = 0;
float debug1; //generic debug value
float debug2; // generic debug value

bool LoadMap()
{
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


void DrawViews()
{

	// *** DRAW VIEWS **


	// ** DRAW AbsoluteView VIEWPORT **

	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(m_renderer, &AbsoluteView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(m_renderer, &AbsoluteView);

	
	// ** DRAW TRANSFORMED VIEWPORT **

	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(m_renderer, &TransformedView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(m_renderer, &TransformedView);

	Offset.x = TransformedView.x;
	Offset.y = TransformedView.y;

	// Draw player line
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(ViewWidth/2, ViewHeight/2, ViewWidth / 2, ViewWidth / 2 - 5, Offset);

	// Change render colour to a light gray for intersect lines
	SDL_SetRenderDrawColor(m_renderer, 128, 128, 128, 128);

	// Draw intersect line
	DrawLineWithOffset(ViewWidth / 2 + -0.0001, ViewHeight / 2 - 0.0001, ViewWidth / 2 + -60, ViewHeight / 2 - 2, Offset);
	DrawLineWithOffset(ViewWidth / 2 + 0.0001, ViewHeight / 2 - 0.0001, ViewWidth / 2 + 60, ViewHeight / 2 - 2, Offset);

}


void HandleInput()
{
	SDL_PollEvent(&event);


	Player CurrentPos = player;

	// Keyboard input
	switch (event.type) {
	case SDL_KEYDOWN:
		switch (event.key.keysym.sym) {
		case SDLK_RIGHT:
			Angle -= 0.1;
			break;
		case SDLK_LEFT:
			Angle += 0.1;
			break;
		case SDLK_UP:
			player.x += cos(Angle);
			player.y -= sin(Angle);
			if (IsPlayerCollidingWithWall())
			{
				player = CurrentPos;
			}
			break;
		case SDLK_DOWN:
			player.x -= cos(Angle);
			player.y += sin(Angle);
			if (IsPlayerCollidingWithWall())
			{
				player = CurrentPos;
			}
			break;
		case SDLK_w:
			player.x += cos(Angle);
			player.y -= sin(Angle);
			if (IsPlayerCollidingWithWall())
			{
				player = CurrentPos;
			}
			break;
		case SDLK_s:
			player.x -= cos(Angle);
			player.y += sin(Angle);
			if (IsPlayerCollidingWithWall())
			{
				player = CurrentPos;
			}
			break;
		case SDLK_a:
			player.x -= sin(Angle);
			player.y -= cos(Angle);
			if (IsPlayerCollidingWithWall())
			{
				player = CurrentPos;
			}
			break;
		case SDLK_d:
			player.x += sin(Angle);
			player.y += cos(Angle);
			if (IsPlayerCollidingWithWall())
			{
				player = CurrentPos;
			}
			break;
		case SDLK_r:
			player.x = 25;
			player.y = 25;
			Angle = 0.00001f;
			LightPos.x = 5; LightPos.y = 25;
			break;
		case SDLK_t:
			Angle += 45.0 / (180 / M_PI);
			break;
		case SDLK_e:
			Angle += 1.0 / (180 / M_PI);
			break;
		case SDLK_y:
			Angle += 0.000001 / (180 / M_PI);
			break;
		case SDLK_k:
			if (LightPos.x > 0 && LightPos.x <= 100)
				LightPos.x += 1;
			else if (LightPos.x > 100)
				LightPos.x = 1;
			break;
		case SDLK_l:
			if (LightPos.y > 0 && LightPos.y <= 50)
				LightPos.y += 1;
			else if (LightPos.y > 50)
				LightPos.y = 1;
			break;
		case SDLK_LCTRL:
			PlayerHeight = CrouchingHeight;
			break;
		case SDLK_q:
			ContinueGame = false;
			SDL_Quit();
			break;
		}
		break;
	case SDL_KEYUP:
		switch (event.key.keysym.sym) {
		case SDLK_LCTRL:
			PlayerHeight = StandingHeight;
			break;
		}
		break;
	
	}
}


void MainLoop() // Primary game loop.  Structured in this way (separate function) so code can be compiled with emscripten easily.
{
#ifndef __EMSCRIPTEN__
	SDL_Delay(10);
#endif // __EMSCRIPTEN__

	HandleInput();

	// Set color to white & clear
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	SDL_RenderClear(m_renderer);


	WallNo = 1;

	// RenderWalls

	for (WallLine wall : AllWalls)
	{
		RenderWall(wall);
		WallNo++;
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
	SDL_RenderPresent(m_renderer);
}


void DrawDebugText()
{
	// Draw info text
	sprintf(message,
		"Player X is %.2f, Player Y is %.2f. \n"
		"Angle is %.2f degrees or %.2f rads. Cosine (x) is %.2f. Sine (y) is %.2f. \n\n" 
		// " Debug1 val & Debug2 val: %.2f %.2f \n\n"
		"Move with arrow keys / ADSW, r to reset position, e to turn 1 degree, t to turn 45 degrees, k & l to move the light, left ctrl to crouch\nPress q to quit.",
		player.x, player.y, //Player position
		fmod(Angle, 6.28) * 180 / 3.1415926, Angle, cos(Angle), sin(Angle)
		// debug1, debug2
	);

	// Create surfaces, texture & rect needed for text rendering
	SDL_Surface* InfoTextSurface = TTF_RenderText_Blended_Wrapped(font, message, colorRed, 1675);
	SDL_Texture* InfoTexture = SDL_CreateTextureFromSurface(m_renderer, InfoTextSurface);
	InfoTextureWidth = InfoTextSurface->w;
	InfoTextureHeight = InfoTextSurface->h;

	SDL_Rect TextRenderQuad = { 15, 325, InfoTextureWidth, InfoTextureHeight };
	SDL_RenderCopy(m_renderer, InfoTexture, NULL, &TextRenderQuad);

	// destroy data used to draw text
	SDL_FreeSurface(InfoTextSurface);
	SDL_DestroyTexture(InfoTexture);

	DrawingLastWall = false;
}


void RenderDebug(WallLine wallLine)
{

	SDL_Point AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line.
	SDL_Point AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line.

	Vector3 TransformedLineP1 = { 0.0, 0.0 };  // used for drawing the current line.
	Vector3 TransformedLineP2 = { 0.0, 0.0 };  // used for drawing the current line.

	Vector2 IntersectPoint1 = { 0.0, 0.0 };
	Vector2 IntersectPoint2 = { 0.0, 0.0 };

	WallData Wall = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; // Data used to draw the wall.  This structure holds the values of the '3d wall' projected on to the 2d 'camera'.

	AbsoluteLineP1.x = wallLine.PT1x;
	AbsoluteLineP1.y = wallLine.PT1y;
	AbsoluteLineP2.x = wallLine.PT2x;
	AbsoluteLineP2.y = wallLine.PT2y;

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

	float LightScaler = (1.00 / MaxLightDistance) * abs(LightDistance);
	LightScaler = WallToLightPerpendicularity - LightScaler;
	LightScaler = Clamp(LightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.


	if (DrawingLastWall == true)
	{
		DrawDebugText(); // Debug text only applies to one wall
	}

	// ************* DRAW DEBUG VIEWPORTS *************

	//ABSOLUTE VIEW

	Offset.x = AbsoluteView.x;
	Offset.y = AbsoluteView.y;

	// Change render colour to Green.
	SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);

	// Draw Wall / Line in 'Absolute View'
	DrawLineWithOffset(AbsoluteLineP1.x, AbsoluteLineP1.y, AbsoluteLineP2.x, AbsoluteLineP2.y, Offset);

	// Draw player line with red
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(player.x, player.y, player.x + cos(Angle) * 5, player.y - sin(Angle) * 5, Offset);

	// Change render colour to white for drawing light position
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	DrawPixelWithOffset(LightPos.x, LightPos.y, Offset);


	// Draw line from wall to light
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	SDL_RenderDrawLine(m_renderer,
		Offset.x + MiddleOfWall.x,
		Offset.y + MiddleOfWall.y,
		Offset.x + MiddleOfWall.x + (VectorToLightNormalised.x * 5),
		Offset.y + MiddleOfWall.y + (VectorToLightNormalised.y * 5)
	);


	// Draw wall normal
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawLine(m_renderer,
		Offset.x + MiddleOfWall.x,
		Offset.y + MiddleOfWall.y,
		Offset.x + MiddleOfWall.x + (WallNormal.x * 5),
		Offset.y + MiddleOfWall.y + (WallNormal.y * 5)
	);


	// TRANSFORMED VIEW

	Offset.x = TransformedView.x;
	Offset.y = TransformedView.y;

	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);



	// Draw wall / line.
	DrawLineWithOffset((ViewWidth / 2 - TransformedLineP1.x), (ViewHeight / 2 - TransformedLineP1.z), (ViewWidth / 2 - TransformedLineP2.x), (ViewHeight / 2 - TransformedLineP2.z), Offset);


	DrawPixelWithOffset(
		(float)ViewWidth / 2 - TransformedLightPos.x,
		(float)ViewHeight / 2 - TransformedLightPos.y,
		Offset);


}



void RenderWall(WallLine wallLine)
{

	SDL_Point AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line.
	SDL_Point AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line.

	Vector3 TransformedLineP1 = { 0.0, 0.0 };  // used for drawing the current line.
	Vector3 TransformedLineP2 = { 0.0, 0.0 };  // used for drawing the current line.

	Vector2 IntersectPoint1 = { 0.0, 0.0 };
	Vector2 IntersectPoint2 = { 0.0, 0.0 };

	WallData Wall = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; // Data used to draw the wall.  This structure holds the values of the '3d wall' projected on to the 2d 'camera'.


	AbsoluteLineP1.x = wallLine.PT1x; // Start by assigning the line points of the current 'wall' to AbsoluteLine
	AbsoluteLineP1.y = wallLine.PT1y;
	AbsoluteLineP2.x = wallLine.PT2x;
	AbsoluteLineP2.y = wallLine.PT2y;



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


	if (TransformedLineP1.z > 0.0 || TransformedLineP2.z > 0.0) // If either point of the wall is in front of the player, draw the wall.
	{

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
		}


		// *** PERSPECTIVE DIVIDE *** What are FOV values? HFOV = 53.13 degrees according to calcs. VFOV = ?

		// x and y scalars are set depending on window height and width

		int xscale = PerspectiveView.w;
		int yscale = PerspectiveView.h * 8;

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

		// Calculate light position.
		Vector2 TransformedLightPos;

		TransformedLightPos.x = (player.y - LightPos.y) * cos(Angle) - (LightPos.x - player.x) * sin(Angle);
		TransformedLightPos.y = (player.y - LightPos.y) * sin(Angle) + (LightPos.x - player.x) * cos(Angle);






		float LeftMostWallPreClip = min(Wall.x1, Wall.x2);
		float RightMostWallPreClip = max(Wall.x1, Wall.x2);

		float WallWidthPreClip = RightMostWallPreClip - LeftMostWallPreClip;

		float AmountOfWallLeftClipped = 0.0, AmountOfWallRightClipped = 0.0; // Used to lighting calcs.  Lighting calcs are performed on the entire wall but often only a portion of the wall is shown so we need to know how much of wall was clipped.
		
		// Clip walls with left & right side of view pane
		if (Wall.x1 <= -WindowWidth / 2 + 1) // clip wall pt1 left
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, -WindowWidth / 2 + 1, -WindowHeight / 2, -WindowWidth / 2 + 1, WindowHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, -WindowWidth / 2 + 1, -WindowHeight / 2, -WindowWidth / 2 + 1, WindowHeight / 2);
			AmountOfWallLeftClipped = Wall.x1 - PerspectiveViewClipPosTop.x;
			Wall.x1 = PerspectiveViewClipPosTop.x;
			Wall.y1a = PerspectiveViewClipPosTop.y;
			Wall.y1b = PerspectiveViewClipPosBottom.y;
		}
		
		if (Wall.x2 <= -WindowWidth / 2 + 1) // clip wall pt2 left
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, -WindowWidth / 2 + 1, -WindowHeight / 2, -WindowWidth / 2 + 1, WindowHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, -WindowWidth / 2 + 1, -WindowHeight / 2, -WindowWidth / 2 + 1, WindowHeight / 2);
			AmountOfWallLeftClipped = Wall.x2 - PerspectiveViewClipPosTop.x;
			Wall.x2 = PerspectiveViewClipPosTop.x;
			Wall.y2a = PerspectiveViewClipPosTop.y;
			Wall.y2b = PerspectiveViewClipPosBottom.y;
		}
		
		if (Wall.x1 >= WindowWidth / 2 - 1) // clip wall pt1 right
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, WindowWidth / 2 - 2, -WindowHeight / 2, WindowWidth / 2 - 2, WindowHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, WindowWidth / 2 - 2, -WindowHeight / 2, WindowWidth / 2 - 2, WindowHeight / 2);
			AmountOfWallRightClipped = Wall.x1 - PerspectiveViewClipPosTop.x;
			Wall.x1 = PerspectiveViewClipPosTop.x;
			Wall.y1a = PerspectiveViewClipPosTop.y;
			Wall.y1b = PerspectiveViewClipPosBottom.y;
		}

		if (Wall.x2 >= WindowWidth / 2 - 1) // clip wall pt2 right
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, WindowWidth / 2 - 2, -WindowHeight / 2, WindowWidth / 2 - 2, WindowHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, WindowWidth / 2 - 2, -WindowHeight / 2, WindowWidth / 2 - 2, WindowHeight / 2);
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

		// debug info 
		debug1 = WallStartY;
		debug2 = WallEndY;

		float WallTotalVisibleXChange = WallEndX - WallStartX;
		float WallTotalVisibleYChange = WallEndY - WallStartY;

		Vector2 VectorToLight;
		float DistanceToLight;

		if (WallWidth != 0)
		{

			float StepXDelta = WallTotalVisibleXChange / WallWidth;
			float StepYDelta = WallTotalVisibleYChange / WallWidth;

			for (int cl = LeftMostWall; cl <= RightMostWall; ++cl) // Loop over each x position of the wall. cl = current vertical line
			{

				
				float CurrentXWallPoint = WallStartX + (StepXDelta * (cl - LeftMostWall));
				float CurrentYWallPoint = WallStartY + (StepYDelta * (cl - LeftMostWall));

				VectorToLight = { LightPos.x - CurrentXWallPoint, LightPos.y - CurrentYWallPoint };
				DistanceToLight = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

				// Normalise the VectorToLight
				VectorToLight = { VectorToLight.x / DistanceToLight , VectorToLight.y / DistanceToLight };

				Vector3 WallNormal = Cross( // What happens if one of these are 0?
					Vector3(StepXDelta * (cl - LeftMostWall), StepYDelta * (cl - LeftMostWall), 0),
					Vector3(0, 0, -1)
				);

				WallNormal.Normalise();

				float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLight);

				float LightScaler = (1.00 / MaxLightDistance) * abs(DistanceToLight);
				LightScaler = WallToLightPerpendicularity - LightScaler;
				LightScaler = Clamp(LightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.



				YDeltaTop = (cl - LeftMostWall) * (HeightDeltaTop / WallWidth); // Calculate the change in Y position for the current vertical line.  Used for the ceiling vertical line.
				YDeltaBottom = (cl - LeftMostWall) * (HeightDeltaBottom / WallWidth); // Calculate the change in Y position for the current vertical line.  Used for the floor vertical line.

				float WallDrawTop = Wall.y1a + YDeltaTop; // Calculate the Y position to draw the vertical ceiling line to.
				if (WallDrawTop < -WindowHeight / 2) // Set a minimum for this value to ensure vertical ceiling line at least starts at the top of the viewport and is not drawn above that (non-visible area)
					WallDrawTop = -WindowHeight / 2;

				float WallDrawBottom = Wall.y1b - YDeltaBottom; // Calculate the Y position to draw the vertical floor line from.
				if (WallDrawBottom > WindowHeight / 2) // Same as above but sets a maximum.
					WallDrawBottom = WindowHeight / 2;


				// Change render colour to the ceiling color.  Lines are drawn relative to the centre of the player's view.
				SDL_SetRenderDrawColor(m_renderer, CeilingColor.r, CeilingColor.g, CeilingColor.b, CeilingColor.a);
				DrawLineWithOffset(WindowWidth / 2 + cl, WindowHeight / 2 + WallDrawTop, WindowWidth / 2 + cl, 1, Offset);



				// Change render colour to the floor color.  Lines are drawn relative to the centre of the player's view.
				SDL_SetRenderDrawColor(m_renderer, FloorColor.r, FloorColor.g, FloorColor.b, FloorColor.a);
				DrawLineWithOffset(WindowWidth / 2 + cl, WindowHeight / 2 + WallDrawBottom, WindowWidth / 2 + cl, WindowHeight - 2, Offset);
				
	
				// WallStep = LightStep;
				// d_TotalWallWidth = TotalWallWidth;

				if (isnan(LightScaler))
				{
					LightScaler = 0.0;
				}

				int R = wallLine.wallColor.r * LightScaler;
				int G = wallLine.wallColor.g * LightScaler;
				int B = wallLine.wallColor.b * LightScaler;


				// Draw Wall
				// Change render colour to the wall color & apply lighting.  Lines are drawn relative to the centre of the player's view.

				// SDL_SetRenderDrawColor(m_renderer, wallLine.wallColor.r, wallLine.wallColor.g, wallLine.wallColor.b, wallLine.wallColor.a);
				SDL_SetRenderDrawColor(m_renderer, R, G, B, wallLine.wallColor.a);
				DrawLineWithOffset(WindowWidth / 2 + cl, WindowHeight / 2 + WallDrawTop, WindowWidth / 2 + cl, WindowHeight / 2 + WallDrawBottom, Offset);
			}


			// DRAW LIGHT SOURCE PIXEL

			Vector2 LightPixel =
			{
				-(TransformedLightPos.x * xscale) / (TransformedLightPos.y),  // Perspective divides
				-yscale / TransformedLightPos.y
			};

			SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
			DrawPixelWithOffset(WindowWidth / 2 + LightPixel.x, WindowHeight / 2 + LightPixel.y, Offset);


			// Change render colour to green
			SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);


			// Draw top line of wall
			DrawLineWithOffset(WindowWidth / 2 + Wall.x1, WindowHeight / 2 + Wall.y1a, WindowWidth / 2 + Wall.x2, WindowHeight / 2 + Wall.y2a, Offset);

			// Draw bottom line of wall
			DrawLineWithOffset(WindowWidth / 2 + Wall.x1, WindowHeight / 2 + Wall.y1b, WindowWidth / 2 + Wall.x2, WindowHeight / 2 + Wall.y2b, Offset);

			// Draw left line of wall
			DrawLineWithOffset(WindowWidth / 2 + Wall.x1, WindowHeight / 2 + Wall.y1a, WindowWidth / 2 + Wall.x1, WindowHeight / 2 + Wall.y1b, Offset);

			// Draw right line of wall
			DrawLineWithOffset(WindowWidth / 2 + Wall.x2, WindowHeight / 2 + Wall.y2a, WindowWidth / 2 + Wall.x2, WindowHeight / 2 + Wall.y2b, Offset);
		}

	}
		
}



int main(int argc, char * argv[])
{

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	TTF_Init();

	if (SDL_CreateWindowAndRenderer(WindowWidth, WindowHeight, 0, &m_window, &m_renderer) < 0)
	{
		ContinueGame = false;
	}
	font = TTF_OpenFont("font.ttf", 16);

	if (font == NULL || LoadMap() == false)
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
