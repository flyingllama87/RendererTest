#include <string>
#include <cmath>
#include <SDL.h>
#include <SDL_ttf.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/*
WARNING:  This code is messy & purely PoC.  You've been warned.
*/


// **** DATA STRUCTURES ****

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
		x = PassedX; y = PassedY; z = 0;
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
void DrawLineWithOffset(int x1, int y1, int x2, int y2, SDL_Point offset); 
void DrawPixelWithOffset(float x, float y, SDL_Point offset);
// Offset functions make it easy to have multiple viewports

float Dot(Vector2 first, Vector2 second); // Calculate vector2 dot product.
Vector3 Cross(Vector3 first, Vector3 second); // Calculate Vector 3 cross product.
float Clamp(float Clampee, float MinVal, float MaxVal);


bool PlayerInBounds(Vector2 WallPt1, Vector2 WallPt2);
bool IsPlayerCollidingWithWall();




// **** GLOBAL DATA ****

SDL_Color colorRed = { 255, 0, 0, 255 }; // used for text

SDL_Event event; // Used for tracking SDL events.


char message[3000] = ""; // used for debug info
int InfoTextureWidth; // used for debug info
int InfoTextureHeight; // used for debug info

bool VertexInPlayerViewPT1 = false, VertexBehindPlayerPT1 = false;  // used for debug info
bool VertexInPlayerViewPT2 = false, VertexBehindPlayerPT2 = false;  // used for debug info

bool DrawingFirstWall = true;

bool ContinueGame = true; // used for exit

double Angle = 0.0000001f; // starting angle for player.

SDL_Point Offset; // used for setting an 'offset' for each view.

const int ViewWidth = 300;
const int ViewHeight = 300;

// x, y, w, h
SDL_Rect AbsoluteView = { 30, 15, ViewWidth, ViewHeight };
SDL_Rect RotatedView = { 360, 15, ViewWidth, ViewHeight };
SDL_Rect TransformedView = { 690, 15, ViewWidth, ViewHeight };
SDL_Rect PixelView = { 1020, 15, ViewWidth, ViewHeight };
SDL_Rect PerspectiveView = { 1350, 15, ViewWidth, ViewHeight };

Player player = { 10, 10 }; // could be moved to vec2. Starting pos. defined.

SDL_Point AbsoluteLineP1 = { 0, 0 }; // used for drawing the current line.
SDL_Point AbsoluteLineP2 = { 0, 0 }; // used for drawing the current line.

SDL_Point RotatedLineP1 = { 0, 0 };  // used for drawing the current line.
SDL_Point RotatedLineP2 = { 0, 0 };  // used for drawing the current line.

Vector3 TransformedLineP1 = { 0.0, 0.0 };  // used for drawing the current line.
Vector3 TransformedLineP2 = { 0.0, 0.0 };  // used for drawing the current line.

Vector3 OldTransformedLineP1 = { 0.0, 0.0 };  // used for drawing the current line.
Vector3 OldTransformedLineP2 = { 0.0, 0.0 };  // used for drawing the current line.

Vector2 IntersectPoint1 = { 0.0, 0.0 };
Vector2 IntersectPoint2 = { 0.0, 0.0 };

int CrouchingHeight = 4 * (PerspectiveView.h / 4); // Crouching Height
int StandingHeight = 10 * (PerspectiveView.h / 4); // standing height.

int PlayerHeight = StandingHeight; // Start standing.

WallData Wall = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; // Data used to draw the wall.  This structure holds the values of the '3d wall' projected on to the 2d 'camera'.

int c = 0; // used to keep track of current verticle line to draw

// self explanatory:

SDL_Color CeilingColor = { 64,64,64,255 };
SDL_Color FloorColor = { 224,224,224,255 };


// Walls must be defined in a clockwise 'winding order'
WallLine wall1 = { 50.0, 0.0, 100.0, 25.0, { 255, 0, 255, 255 } };
WallLine wall2 = { 100.0, 25.0, 50.0, 50.0, { 255, 0, 255, 255 } };
WallLine wall3 = { 0.0, 0.0, 50.0, 0.0, { 255, 0, 255, 255 } };
WallLine wall4 = { 50.0, 50.0, 0.0, 50.0, { 255, 0, 255, 255 } };
WallLine wall5 = { 0.0, 50.0, 0.0, 0.0, { 255, 0, 255, 255 } };
// WallLine wall6 = {}

float LightSize = 50.0;

Vector2 LightPos = { (float)25, (float)25 };
float MaxLightDistance = 100;
float LightFalloff = 1; // Light intensity diminishes by a value of 1 per pixel.

float TempLightDistance = 0;

// SDL vars:
TTF_Font *font;
SDL_Window *m_window;
SDL_Renderer *m_renderer;


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


	// ** DRAW ROTATED VIEWPORT **  View rotated counter clockwise 90 degrees

	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(m_renderer, &RotatedView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(m_renderer, &RotatedView);


	// ** DRAW TRANSFORMED VIEWPORT **  The co-ordinates have to change due to this new view which is now looking at the world from the player's perspective.

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



	// ** DRAW PIXELVIEW VIEWPORT **



	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(m_renderer, &PixelView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(m_renderer, &PixelView);

	Offset.x = PixelView.x;
	Offset.y = PixelView.y;

	// Draw player line
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(ViewWidth / 2, ViewHeight / 2, ViewWidth / 2, ViewWidth / 2 - 5, Offset);

	// * Intersect lines used by perspective view *

	// Change render colour to a light gray for intersect lines
	SDL_SetRenderDrawColor(m_renderer, 128, 128, 128, 128);

	// Draw intersect line
	DrawLineWithOffset(ViewWidth / 2 + -0.0001, ViewHeight / 2 - 0.0001, ViewWidth / 2 + -60, ViewHeight / 2 - 2, Offset);
	DrawLineWithOffset(ViewWidth / 2 + 0.0001, ViewHeight / 2 - 0.0001, ViewWidth / 2 + 60, ViewHeight / 2 - 2, Offset);




	// *** DRAW PERSPECTIVE VIEWPORT ***

	// Set color to black & draw viewport background
	SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(m_renderer, &PerspectiveView);

	// Set color to red and draw border.
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(m_renderer, &PerspectiveView);

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
			break;
		case SDLK_w:
			player.x += cos(Angle);
			player.y -= sin(Angle);
			break;
		case SDLK_s:
			player.x -= cos(Angle);
			player.y += sin(Angle);
			break;
		case SDLK_a:
			player.x -= sin(Angle);
			player.y -= cos(Angle);
			break;
		case SDLK_d:
			player.x += sin(Angle);
			player.y += cos(Angle);
			break;
		case SDLK_r:
			player.x = 10;
			player.y = 10;
			Angle = 0.0000000;
			LightPos.x = 25; LightPos.y = 25;
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
		case SDLK_l:
			if (LightPos.y > 0 && LightPos.y <= 100)
				LightPos.y += 1;
			else if (LightPos.y > 100)
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


void MainLoop()
{
#ifndef __EMSCRIPTEN__
	SDL_Delay(10);
#endif // __EMSCRIPTEN__

	HandleInput();

	// Set color to white & clear
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	SDL_RenderClear(m_renderer);

	// Set up viewports

	DrawViews();

	// RenderWalls

	DrawingFirstWall = true; // Used to determine when the first wall is being drawn so debug info can be drawn.  Switch to false after first draw.
	RenderWall(wall1);
	RenderWall(wall2);
	RenderWall(wall3);
	RenderWall(wall4);
	RenderWall(wall5);

	// perform render
	SDL_RenderPresent(m_renderer);


}


void DrawDebugText()
{
	// Draw info text
	sprintf(message,
		"  Absolute View                    Rotated View                     Transform View                   Pixel View                       Perspective View\n\n" // Hacky
		"Player X is %.2f, Player Y is %.2f. \n"
		"Angle is %.2f degrees or %.2f rads. Cosine (x) is %.2f. Sine (y) is %.2f. \n\n"
		"TrnsfmPT1: X %.2f, Y %.2f, Z %.2f. \n"
		"TrnsfmPT2: X %.2f, Y %.2f, Z %.2f. \n"
		"Relative : X %.2f, Y %.2f, Z %.2f. Note: Z val is used for Y pos in transform view. \n"
		"to view  : X %.2f, Y %.2f, Z %.2f. \n\n"
		"Wall Data: X1 %.2f, Y1A %.2f, Y1B %.2f. \n"
		"         : X2 %.2f, Y2A %.2f, Y2B %.2f. \n"
		"Relative : X1 %.2f, Y1A %.2f, Y1B %.2f. \n"
		"to view  : X2 %.2f, Y2A %.2f, Y2B %.2f. \n\n"
		"Line Data: top   : X1 %.2f, Y1A %.2f / X2 %.2f, Y2A %.2f \n"
		"Line Data: bottom: X1 %.2f, Y1B %.2f / X2 %.2f, Y2B %.2f \n"
		"Line Data: left  : X1 %.2f, Y1A %.2f / X1 %.2f, Y1B %.2f \n"
		"Line Data: right : X2 %.2f, Y2A %.2f / X2 %.2f, Y2B %.2f \n\n"
		"InterSPT1: X: %.2f Z: %.2f PT1 USED: %s PT2 USED: %s \n"
		"InterSPT2: X: %.2f Z: %.2f PT1 USED: %s PT2 USED: %s \n"
		"LightDistance: %.2f \n\n"
		"Move with arrow keys / ADSW, r to reset position, e to turn 1 degree, t to turn 45 degrees, left ctrl to crouch\nPress q to quit.",
		player.x, player.y, //Player position
		fmod(Angle, 6.28) * 180 / 3.1415926, Angle, cos(Angle), sin(Angle), // Angle data
		OldTransformedLineP1.x, OldTransformedLineP1.y, OldTransformedLineP1.z, // Transform line p1
		OldTransformedLineP2.x, OldTransformedLineP2.y, OldTransformedLineP2.z, // Transform line p2
		ViewWidth / 2 - TransformedLineP1.x, ViewHeight / 2 - TransformedLineP1.y, ViewHeight / 2 - TransformedLineP1.z, // Transform line p1
		ViewWidth / 2 - TransformedLineP2.x, ViewHeight / 2 - TransformedLineP2.y, ViewHeight / 2 - TransformedLineP2.z, // Transform line p2
		Wall.x1, Wall.y1a, Wall.y1b, // Wall data raw
		Wall.x2, Wall.y2a, Wall.y2b, // Wall data raw
		ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1a, ViewHeight / 2 + Wall.y1b, // wall data relative to player/centre of screen
		ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2a, ViewHeight / 2 + Wall.y2b, // wall data relative to player/centre of screen
		ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1a, ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2a, // top line
		ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1b, ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2b, // bottom line
		ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1a, ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1b, // left line
		ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2a, ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2b,  // right line
		IntersectPoint1.x, IntersectPoint1.y, VertexInPlayerViewPT1 ? "Yes" : "No", VertexInPlayerViewPT2 ? "Yes" : "No",
		IntersectPoint2.x, IntersectPoint2.y, VertexBehindPlayerPT1 ? "Yes" : "No", VertexBehindPlayerPT2 ? "Yes" : "No",
		TempLightDistance
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

	DrawingFirstWall = false;
}






void RenderWall(WallLine wallLine)
{

	AbsoluteLineP1.x = wallLine.PT1x;
	AbsoluteLineP1.y = wallLine.PT1y;
	AbsoluteLineP2.x = wallLine.PT2x;
	AbsoluteLineP2.y = wallLine.PT2y;

	VertexInPlayerViewPT1 = false; VertexInPlayerViewPT2 = false;
	VertexBehindPlayerPT1 = false; VertexBehindPlayerPT2 = false;



	//ABSOLUTE VIEW

	Offset.x = AbsoluteView.x;
	Offset.y = AbsoluteView.y;

	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);

	// Draw first line.
	DrawLineWithOffset(AbsoluteLineP1.x, AbsoluteLineP1.y, AbsoluteLineP2.x, AbsoluteLineP2.y, Offset);

	// Draw player line
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(player.x, player.y, player.x + cos(Angle) * 5, player.y - sin(Angle) * 5, Offset);

	// Change render colour to white for drawing light position
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	DrawPixelWithOffset(LightPos.y, LightPos.x, Offset);



	// ROTATED VIEW

	Offset.x = RotatedView.x;
	Offset.y = RotatedView.y;

	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);


	// set line to be relative to player.
	RotatedLineP1.x = AbsoluteLineP1.y;
	RotatedLineP1.y = ViewHeight - AbsoluteLineP1.x;

	RotatedLineP2.x = AbsoluteLineP2.y;
	RotatedLineP2.y = ViewHeight - AbsoluteLineP2.x;

	Vector2 RotatatedPlayerPosition;
	RotatatedPlayerPosition.x = player.y;
	RotatatedPlayerPosition.y = player.x;

	// Draw wall / line.
	DrawLineWithOffset(RotatedLineP1.x, RotatedLineP1.y, RotatedLineP2.x, RotatedLineP2.y, Offset);

	// Draw player line
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	DrawLineWithOffset(RotatatedPlayerPosition.x, ViewHeight - RotatatedPlayerPosition.y, RotatatedPlayerPosition.x - sin(Angle) * 5, ViewHeight - (RotatatedPlayerPosition.y + cos(Angle) * 5), Offset);



	// TRANSFORMED VIEW

	Offset.x = TransformedView.x;
	Offset.y = TransformedView.y;

	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);


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

	// Draw wall / line.
	DrawLineWithOffset((ViewWidth/2 - TransformedLineP1.x), (ViewHeight / 2 - TransformedLineP1.z), (ViewWidth / 2 - TransformedLineP2.x), (ViewHeight / 2 - TransformedLineP2.z), Offset);

	OldTransformedLineP1 = TransformedLineP1;
	OldTransformedLineP2 = TransformedLineP2;
	
	// LIGHTING CALCULATIONS (2D / Height not taken into account)

	Vector2 TransformedLightPos;

	TransformedLightPos.x = (player.y - LightPos.x) * cos(Angle) + ( LightPos.y - player.x) * -sin(Angle);
	TransformedLightPos.y = (player.y - LightPos.x) * sin(Angle) + (LightPos.y - player.x) * cos(Angle);

	DrawPixelWithOffset(
		(float)ViewWidth / 2 - TransformedLightPos.x,
		(float)ViewHeight / 2 - TransformedLightPos.y,
		Offset);

	Vector2 MiddleOfWall;

	float WallXDistance = TransformedLineP1.x - TransformedLineP2.x;
	float WallYDistance = TransformedLineP1.z - TransformedLineP2.z;

	float HalfOfWallXDistance = WallXDistance / 2;
	float HalfOfWallYDistance = WallYDistance / 2;

	MiddleOfWall.x = TransformedLineP1.x - HalfOfWallXDistance;
	MiddleOfWall.y = TransformedLineP1.z - HalfOfWallYDistance;

	// Draw middle of wall pixel
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	DrawPixelWithOffset((ViewWidth/2) - MiddleOfWall.x, (ViewHeight/2) - MiddleOfWall.y, Offset);
	
	Vector2 VectorToLight = { MiddleOfWall.x - TransformedLightPos.x, MiddleOfWall.y - TransformedLightPos.y };
	float LightDistance = sqrt(VectorToLight.x * VectorToLight.x + VectorToLight.y * VectorToLight.y);

	Vector2 VectorToLightNormalised = { VectorToLight.x / LightDistance, VectorToLight.y / LightDistance };

	// Draw line from wall to light
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	SDL_RenderDrawLine(m_renderer,
		Offset.x + (ViewWidth / 2) - MiddleOfWall.x,
		Offset.y + (ViewHeight / 2) - MiddleOfWall.y,
		Offset.x + (ViewWidth / 2) - MiddleOfWall.x + (VectorToLightNormalised.x * 5),
		Offset.y + (ViewHeight / 2) - MiddleOfWall.y + (VectorToLightNormalised.y * 5)
	);

	Vector3 WallNormal = Cross(
		Vector3(HalfOfWallXDistance, HalfOfWallYDistance, 0),
		Vector3(0, 0, -1)
	);

	WallNormal.Normalise();

	float WallToLightPerpendicularity = Dot(Vector2(WallNormal.x, WallNormal.y), VectorToLightNormalised);

	// Draw wall normal
	SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
	SDL_RenderDrawLine(m_renderer,
		Offset.x + (ViewWidth / 2) - MiddleOfWall.x,
		Offset.y + (ViewHeight / 2) - MiddleOfWall.y,
		Offset.x + (ViewWidth / 2) - MiddleOfWall.x + (WallNormal.x * 5),
		Offset.y + (ViewHeight / 2) - MiddleOfWall.y + (WallNormal.y * 5)
	);

	

	TempLightDistance = WallToLightPerpendicularity;

	float LightScaler = (1.00 / MaxLightDistance) * abs(LightDistance);
	LightScaler = WallToLightPerpendicularity - LightScaler;
	LightScaler = Clamp(LightScaler, 0.33, 1.0); // Min value of light scaler is 0.33 / equiv to ambient light.
	

	// ** DRAW PIXEL VIEW ** 

	Offset.x = PixelView.x;
	Offset.y = PixelView.y;

	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);

	Vector2 PlayerRotate;
	PlayerRotate.x = player.y;
	PlayerRotate.y = player.x;

	// Draw Pixel

	// WIP / simplist
	/*
	DrawPixelWithScaleAndOffset(
	ViewWidth / 2 - (PlayerRotate.x - RotatedLineP1.x),
	PlayerRotate.y - RotatedLineP1.y,
	Offset);
	*/

	/*
	// WIP / simplist w/ rotation
	DrawPixelWithScaleAndOffset(
	ViewWidth / 2 - (PlayerRotate.x - RotatedLineP1.x) * cos(Angle),
	ViewHeight / 2 - (PlayerRotate.y - RotatedLineP1.y) * sin(Angle),
	Offset);
	*/

	/*
	// WIP / rotation & correct positioning but ellipsis
	DrawPixelWithScaleAndOffset(
	ViewWidth / 2 - (PlayerRotate.x - RotatedLineP1.x) * cos(Angle),
	ViewHeight / 2 - ((PlayerRotate.y - RotatedLineP1.y) * sin(Angle) + ((ViewHeight - PlayerRotate.y) - RotatedLineP1.y) * cos(Angle)),
	Offset);*/

	/*
	// WIP /rotation & correct positioning and ellipsis
	DrawPixelWithScaleAndOffset(
	ViewWidth / 2 - ((PlayerRotate.x - RotatedLineP1.x) * cos(Angle) + (PlayerRotate.x - RotatedLineP1.x) * sin(Angle)),
	ViewHeight - ((PlayerRotate.y - RotatedLineP1.y) * sin(Angle) + ((ViewHeight - PlayerRotate.y) - RotatedLineP1.y) * cos(Angle)),
	Offset);
	*/

	//  working
	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
	DrawPixelWithOffset(
		(float)ViewWidth/2 - ((PlayerRotate.x - RotatedLineP2.x) * cos(Angle) + ((ViewHeight - PlayerRotate.y) - RotatedLineP2.y) * -sin(Angle)),
		(float)ViewWidth/2 - ((PlayerRotate.x - RotatedLineP2.x) * sin(Angle) + ((ViewHeight - PlayerRotate.y) - RotatedLineP2.y) * cos(Angle)),
		Offset);

	// Draw Arc

	// Change render colour
	SDL_SetRenderDrawColor(m_renderer, 128, 0, 255, 255);

	for (c = 0; c < fmod(abs(Angle), 6.28) * 180 / 3.1415926; c++)
	{
		float c_rad = c * (M_PI / 180);
		DrawPixelWithOffset(
			ViewWidth / 2 + (cos(c_rad) * 75),
			ViewHeight / 2 - (sin(c_rad) * 75),
			Offset);
	}



	// PERSPECTIVE VIEW / PROJECTION

	Offset.x = PerspectiveView.x;
	Offset.y = PerspectiveView.y;


	if (TransformedLineP1.z > 0.0 || TransformedLineP2.z > 0.0) // If either point of the wall is in front of the player, draw the wall.
	{

		IntersectPoint1 = Intersect(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, -0.0001, 0.0001, -60, 2);
		IntersectPoint2 = Intersect(TransformedLineP1.x, TransformedLineP1.z, TransformedLineP2.x, TransformedLineP2.z, 0.0001, 0.0001, 60, 2);

		// If the line is partially behind the player (crosses the viewplane, clip it)

		if (TransformedLineP1.z <= 0.0) // If PT1 is behind the player at all,
		{

			if (IntersectPoint1.y > 0.0)
			{
				VertexInPlayerViewPT1 = true;
				TransformedLineP1.x = IntersectPoint1.x;
				TransformedLineP1.z = IntersectPoint1.y;
			}
			else
			{
				VertexBehindPlayerPT1 = true;
				TransformedLineP1.x = IntersectPoint2.x;
				TransformedLineP1.z = IntersectPoint2.y;
			}
		}

		if (TransformedLineP2.z <= 0.0) // If PT2 is behind the player at all,
		{

			if (IntersectPoint1.y > 0.0)
			{
				VertexInPlayerViewPT2 = true;
				TransformedLineP2.x = IntersectPoint1.x;
				TransformedLineP2.z = IntersectPoint1.y;
			}
			else
			{
				VertexBehindPlayerPT2 = true;
				TransformedLineP2.x = IntersectPoint2.x;
				TransformedLineP2.z = IntersectPoint2.y;
			}
		}

		// *** PERSPECTIVE DIVIDE ***  - NO FOV DIVIDE - THEREFORE VFOV & HFOV ARE 90 DEGREES 

		// x values and y values are scaled depending on viewport height and width

		int xscale = PerspectiveView.w;
		int yscale = PerspectiveView.h * 4;

		if (PlayerHeight == CrouchingHeight)
		{
			yscale = yscale + (StandingHeight - CrouchingHeight);
		}

		Wall.x1 = -(TransformedLineP1.x * xscale) / (TransformedLineP1.z); // Perspective divides to get verticies co-ords of wall to draw.  Artificially multiply by 16 to 'pump' it to good size in leiu of FOV calculations.
		
		Wall.y1a = -yscale / (TransformedLineP1.z / 2);
		Wall.y1b = PlayerHeight / (TransformedLineP1.z / 2);

		Wall.x2 = -(TransformedLineP2.x * xscale) / (TransformedLineP2.z); // Perspective divides
		
		Wall.y2a = -yscale / (TransformedLineP2.z / 2);
		Wall.y2b = PlayerHeight / (TransformedLineP2.z / 2);

		


		
		// Clip walls with left & right side of view pane
		if (Wall.x1 <= -ViewWidth / 2 + 1) // clip wall pt1 left
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, -ViewWidth / 2 + 1, -ViewHeight / 2, -ViewWidth / 2 + 1, ViewHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, -ViewWidth / 2 + 1, -ViewHeight / 2, -ViewWidth / 2 + 1, ViewHeight / 2);
			Wall.x1 = PerspectiveViewClipPosTop.x;
			Wall.y1a = PerspectiveViewClipPosTop.y;
			Wall.y1b = PerspectiveViewClipPosBottom.y;
		}
		
		if (Wall.x2 <= -ViewWidth / 2 + 1) // clip wall pt2 left
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, -ViewWidth / 2 + 1, -ViewHeight / 2, -ViewWidth / 2 + 1, ViewHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, -ViewWidth / 2 + 1, -ViewHeight / 2, -ViewWidth / 2 + 1, ViewHeight / 2);
			Wall.x2 = PerspectiveViewClipPosTop.x;
			Wall.y2a = PerspectiveViewClipPosTop.y;
			Wall.y2b = PerspectiveViewClipPosBottom.y;
		}
		
		if (Wall.x1 >= ViewWidth / 2 - 1) // clip wall pt1 right
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, ViewWidth / 2 - 2, -ViewHeight / 2, ViewWidth / 2 - 2, ViewHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, ViewWidth / 2 - 2, -ViewHeight / 2, ViewWidth / 2 - 2, ViewHeight / 2);
			Wall.x1 = PerspectiveViewClipPosTop.x;
			Wall.y1a = PerspectiveViewClipPosTop.y;
			Wall.y1b = PerspectiveViewClipPosBottom.y;
		}

		if (Wall.x2 >= ViewWidth / 2 - 1) // clip wall pt2 right
		{
			Vector2 PerspectiveViewClipPosTop = Intersect(Wall.x1, Wall.y1a, Wall.x2, Wall.y2a, ViewWidth / 2 - 2, -ViewHeight / 2, ViewWidth / 2 - 2, ViewHeight / 2);
			Vector2 PerspectiveViewClipPosBottom = Intersect(Wall.x1, Wall.y1b, Wall.x2, Wall.y2b, ViewWidth / 2 - 2, -ViewHeight / 2, ViewWidth / 2 - 2, ViewHeight / 2);
			Wall.x2 = PerspectiveViewClipPosTop.x;
			Wall.y2a = PerspectiveViewClipPosTop.y;
			Wall.y2b = PerspectiveViewClipPosBottom.y;
		}


		// Calculations for drawing the verticle lines of the wall, floor and ceiling.

		float YDeltaTop, YDeltaBottom; // Used to calculate y position for current x position of wall.

		float LeftMostWall = min(Wall.x1, Wall.x2);
		float RightMostWall = max(Wall.x1, Wall.x2);
		float HeightDeltaTop, HeightDeltaBottom;

		HeightDeltaTop = Wall.y2a - Wall.y1a;
		HeightDeltaBottom = Wall.y1b - Wall.y2b;

		float linelength = RightMostWall - LeftMostWall;

		if (linelength != 0)
		{
			for (int cl = LeftMostWall; cl <= RightMostWall; ++cl)
			{

				if (HeightDeltaTop != 0)
				{
					YDeltaTop = (cl - LeftMostWall) * (HeightDeltaTop / linelength);
					YDeltaBottom = (cl - LeftMostWall) * (HeightDeltaBottom / linelength);
				}
				else
				{
					YDeltaTop = 0; // Done as the max can be go 'weird' (read: get large numbers) otherwise.
					YDeltaBottom = 0;
				}
					

				float WallDrawTop = Wall.y1a + YDeltaTop; // Set max wall height.
				if (WallDrawTop < -ViewHeight / 2)
					WallDrawTop = -ViewHeight / 2;

				float WallDrawBottom = Wall.y1b - YDeltaBottom; // Set wall height... whatever?.
				if (WallDrawBottom > ViewHeight / 2)
					WallDrawBottom = ViewHeight / 2;

				if (WallDrawBottom != -ViewHeight / 2) // Draw ceiling
				{
					// Change render colour to the ceiling color
					SDL_SetRenderDrawColor(m_renderer, CeilingColor.r, CeilingColor.g, CeilingColor.b, CeilingColor.a);
					DrawLineWithOffset(ViewWidth / 2 + cl, ViewHeight / 2 + WallDrawTop, ViewWidth / 2 + cl, 1, Offset);
				}

				if (WallDrawTop != ViewHeight / 2) // Draw floor
				{
					// Change render colour to the floor color
					SDL_SetRenderDrawColor(m_renderer, FloorColor.r, FloorColor.g, FloorColor.b, FloorColor.a);
					DrawLineWithOffset(ViewWidth / 2 + cl, ViewHeight / 2 + WallDrawBottom, ViewWidth / 2 + cl, ViewHeight - 2, Offset);
				}

				// Draw Wall
				// Change render colour to the wall color
				SDL_SetRenderDrawColor(m_renderer, wallLine.wallColor.r * LightScaler, wallLine.wallColor.g * LightScaler, wallLine.wallColor.b * LightScaler, wallLine.wallColor.a);
				DrawLineWithOffset(ViewWidth / 2 + cl, ViewHeight / 2 + WallDrawTop, ViewWidth / 2 + cl, ViewHeight / 2 + WallDrawBottom, Offset);
			}


			// DRAW LIGHT SOURCE

			Vector2 LightPixel =
			{
				-(TransformedLightPos.x * xscale) / (TransformedLightPos.y),  // Perspective divides
				-(yscale) / (TransformedLightPos.y / 2)
			};

			SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
			DrawPixelWithOffset(ViewWidth / 2 + LightPixel.x, ViewWidth / 2 + LightPixel.y, Offset);


			// Change render colour to green
			SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);


			// Draw top line of wall
			DrawLineWithOffset(ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1a, ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2a, Offset);

			// Draw bottom line of wall
			DrawLineWithOffset(ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1b, ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2b, Offset);

			// Draw left line of wall
			DrawLineWithOffset(ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1a, ViewWidth / 2 + Wall.x1, ViewHeight / 2 + Wall.y1b, Offset);

			// Draw right line of wall
			DrawLineWithOffset(ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2a, ViewWidth / 2 + Wall.x2, ViewHeight / 2 + Wall.y2b, Offset);
		}

	}

	if (DrawingFirstWall == true)
		DrawDebugText();
}


void DrawLineWithOffset(int x1, int y1, int x2, int y2, SDL_Point offset)
{
	SDL_RenderDrawLine(m_renderer, x1 + offset.x, y1 + offset.y, x2 + offset.x, y2 + offset.y);
}


void DrawPixelWithOffset(float x, float y, SDL_Point offset)
{
	SDL_RenderDrawPoint(m_renderer, x + offset.x, y + offset.y);
}

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


bool IsPlayerCollidingWithWall()
{
	Vector2 WallPt1 = { wall1.PT1x, wall1.PT1y };
	Vector2 WallPt2 = { wall1.PT2x, wall1.PT2y };

	if (!PlayerInBounds(WallPt1, WallPt2))
		return true;

	WallPt1 = { wall2.PT1x, wall2.PT1y };
	WallPt2 = { wall2.PT2x, wall2.PT2y };

	if (!PlayerInBounds(WallPt1, WallPt2))
		return true;

	WallPt1 = { wall3.PT1x, wall3.PT1y };
	WallPt2 = { wall3.PT2x, wall3.PT2y };

	if (!PlayerInBounds(WallPt1, WallPt2))
		return true;

	WallPt1 = { wall4.PT1x, wall4.PT1y };
	WallPt2 = { wall4.PT2x, wall4.PT2y };

	if (!PlayerInBounds(WallPt1, WallPt2))
		return true;

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


int main(int argc, char * argv[])
{

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	TTF_Init();

	SDL_CreateWindowAndRenderer(1680, 1050, 0, &m_window, &m_renderer);
	font = TTF_OpenFont("font.ttf", 16);

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(MainLoop, -1, 1);
#else
	while (ContinueGame) { MainLoop(); }
#endif

	return 0;
}
