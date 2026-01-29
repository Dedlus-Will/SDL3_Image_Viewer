#include <algorithm>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>


////////////////
// Prototypes //
////////////////


void tryLogError();
void loadImage(const char* imgPath);
void refreshRender();
void updateTextureInfo(const char* filePath);
void getImageRes();
void panImage(float mousex, float mousey);
void setStartingRes();
void tryAutoMaxWindow();
void saveMaximizedState();
void handleUserInput();
std::filesystem::path getExecutableDirectory();


///////////////////
// Variable Init //
///////////////////


bool switchImage{ false };
bool exitWindow{ false };
bool maximized{ false };
bool fullscreen{ false };
float imageSize{ 1.0 };
float xLocPan{ 0 };
float yLocPan{ 0 };
int width{ 0 };
int height{ 0 };
int mouseCenteredX{ 0 };
int mouseCenteredY{ 0 };
SDL_Renderer* renderer{ nullptr };
SDL_Texture* imageTex{ nullptr };
SDL_Window* window{ nullptr };


////////////////////
//----- MAIN -----//
////////////////////

int main(int argc, char* argv[])
{
	do
	{
		// Default load on startup
		if (!switchImage)
		{
			// Was filepath passed to program via windows "open with" button?
			if (argc > 1)
			{
				// Load image via Windows Open-With command
				loadImage(argv[1]);
			}
			else
			{
				loadImage("data/Null.png");
			}
		}
		else
		{
			// Switch to a new image on user input
			SDL_Log("Switched image. Defaulting to Null.png (no functionality yet)\n");
			loadImage("data/Null.png");
		}
	} while (!exitWindow);

	return 0;
}


//////////////////////////
// Function Definitions //
//////////////////////////

// Loads an image from a given filepath
void loadImage(const char* imgPath)
{
	// Reset panning
	xLocPan = 0;
	yLocPan = 0;

	// Set width/height to 2/3 of screen res
	setStartingRes();

	// Create the SDL window, then hide it
	if (window == nullptr)
	{
		window = SDL_CreateWindow("Image Viewer", width, height, SDL_WINDOW_HIDDEN);
		SDL_Log("Created window");
	}

	// Make sure relative mouse mode is set properly
	SDL_SetWindowRelativeMouseMode(window, false);

	// Create the renderer
	renderer = SDL_CreateRenderer(window, nullptr);
	SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	// Load the texture
	imageTex = IMG_LoadTexture(renderer, imgPath);
	tryLogError();
	
	// Update saved image resolution
	getImageRes();

	// Clear render and push backbuffer to front
	refreshRender();

	// Show window once file is loaded
	SDL_ShowWindow(window);
	SDL_SetWindowResizable(window, true);
	
	// Maximize window if it was maximized last time program was open
	tryAutoMaxWindow();

	// Update title bar image info
	updateTextureInfo(imgPath);

	// Close program if inputted file is not an image
	if (imageTex == nullptr)
	{
		SDL_Log("No Valid Image");
		SDL_Quit();
		exitWindow = true;
		return;
	}

	// Input loop
	handleUserInput();

	saveMaximizedState();
	SDL_DestroyTexture(imageTex);
	SDL_Quit();

	return;
}

// Hub for handling all user input
void handleUserInput()
{
	bool mouseDown{ false };
	const float scrollSensitivity{ 0.125 };
	float scrollSensitivityScaled{ 1 };

	while (!exitWindow)
	{
		// SDL Events (Used to handle updates)
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED: // Close window
				SDL_Log("close\n");
				exitWindow = true;
				return;
		
			case SDL_EVENT_WINDOW_MOVED: // Finish move or resize
				refreshRender();
				SDL_Log("Window Changed\n");
				break;

			case SDL_EVENT_WINDOW_MAXIMIZED: // Maximize window
				maximized = true;
				saveMaximizedState();
				SDL_Log("Maximized window\n");
				break;

			case SDL_EVENT_WINDOW_RESTORED: // Un-maximize window
				maximized = false;
				saveMaximizedState();
				SDL_Log("Restored window\n");
				break;
			
			case SDL_EVENT_MOUSE_WHEEL: // Scroll
				SDL_Log("Mouse Wheel Input\n");
				scrollSensitivityScaled = scrollSensitivity * std::lerp(imageSize, 4, 1); // Scales sensitivity based on current zoom to keep speed consistent
				imageSize = std::clamp((imageSize + (event.wheel.y * -1) * (scrollSensitivity * scrollSensitivityScaled)), 0.1f, 1.0f);
				panImage(0, 0);
				refreshRender();
				break;
			
			case SDL_EVENT_KEY_DOWN: // Press keyboard key (will be used for img switching)
				SDL_Log("Key Press\n");
				if (event.key.key == SDLK_F11) // F11 Key
				{
					fullscreen = !fullscreen;
					SDL_SetWindowFullscreen(window, fullscreen);
				}
				else if (event.key.key == SDLK_ESCAPE)
				{
					if (fullscreen == true)
					{
						SDL_SetWindowFullscreen(window, false);
						fullscreen = false;
					}
				}
				break;
			
			case SDL_EVENT_MOUSE_BUTTON_DOWN: // Press any mouse button
				mouseDown = true;
				SDL_HideCursor();
				break;
			
			case SDL_EVENT_MOUSE_BUTTON_UP: // Release any mouse button
				mouseDown = false;

				SDL_GetWindowSize(window, &mouseCenteredX, &mouseCenteredY);

				// Move mouse to center of window
				SDL_WarpMouseInWindow(window, mouseCenteredX / 2, mouseCenteredY / 2);
				SDL_ShowCursor();
				break;
			
			case SDL_EVENT_MOUSE_MOTION: // Move mouse
				if (mouseDown)
				{
					panImage(event.motion.xrel, event.motion.yrel);
					refreshRender();
				}
				break;
			}

		}
	}

	return;
}

// Clears render and pushes backbuffer to front. Also handles scaling and panning.
void refreshRender()
{
	// Create rectangle to aid in scaling
	SDL_FRect displayedImgRect = { NULL, NULL, (width * imageSize), (height * imageSize) };

	// Adjust rectangle location to keep image centered
	displayedImgRect.x = ((width * ((imageSize * -1) + 1) / 2) + (xLocPan * -1));
	displayedImgRect.y = ((height * ((imageSize * -1) + 1) / 2) + ((yLocPan * -1) * -1));

	SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	// Set background color
	SDL_SetRenderDrawColor(renderer, 10, 10, 10, 10);

	// Clear render and push backbuffer to front
	SDL_RenderClear(renderer);
	SDL_RenderTexture(renderer, imageTex, &displayedImgRect, NULL);
	SDL_RenderPresent(renderer);

	return;
}

// Updates the current window's title to show the displayed image's filepath and resolution
void updateTextureInfo(const char* filePath)
{
	std::string filePathStr = filePath;
	std::string resolutionStr = std::to_string(width) + "x" + std::to_string(height);

	// Concatenate filepath and resolution to be displayed
	std::string resultFileNameStr = filePathStr + ' ' + '|' + ' ' + resolutionStr;

	// Display file info on window's title bar
	SDL_SetWindowTitle(window, resultFileNameStr.c_str());

	return;
}

// Pans image based on mouse x/y movement
void panImage(float mousex, float mousey)
{
	const float panSensitivity{ 2.25 };

	float maxXPan = ((width / 2) * (imageSize * -1 + 1));
	float maxYPan = ((height / 2) * (imageSize * -1 + 1));
	float minXPan = maxXPan * -1;
	float minYPan = maxYPan * -1;

	float resMultX = static_cast<float>(width) / 1920;
	float resMultY = static_cast<float>(width) / 1080;

	// Scale movement based on zoom amount
	float finalMovementX = ((mousex * imageSize) * (resMultX * panSensitivity));
	float finalMovementY = (((mousey * -1 * 0.5) *imageSize) * (resMultY * panSensitivity)); // Inverted mousey, and scaled down by 0.5


	xLocPan = std::clamp((xLocPan + finalMovementX), minXPan, maxXPan);
	yLocPan = std::clamp((yLocPan + finalMovementY), minYPan, maxYPan);

	return;
}

// Gets image res, then sets global width and height vars
void getImageRes()
{
	// Get image resolution
	float widthf{};
	float heightf{};
	SDL_GetTextureSize(imageTex, &widthf, &heightf);

	// Convert resolution to int
	width = std::truncf(widthf);
	height = std::truncf(heightf);

	return;
}

// UNFINISHED!!! Sets starting res to 2/3 of 1080p, but in future should be 2/3 of whatever the screen res is
void setStartingRes()
{
	int screenWidth{ 1920 };
	int screenHeight{ 1080 };

	width = screenWidth / 3 * 2;
	height = screenHeight / 3 * 2;

	return;
}

// Maximize window if maximize == true in savedata.txt
void tryAutoMaxWindow()
{
	// Find local save dir
	std::string saveDir = getExecutableDirectory().generic_string() + "/data/savedata.txt";

	// load save
	std::ifstream saveFile(saveDir);

	if (saveFile.is_open())
	{
		std::cout << "Loaded file!\n";
		saveFile >> maximized;
	}

	if (maximized)
	{
		SDL_MaximizeWindow(window);
		SDL_Log("Auto-maximized window.\n");
	}
	else
	{
		SDL_Log("Did NOT auto-maximize window.\n");
	}
}

// Set maximized from savedata.txt
void saveMaximizedState()
{
	// Find local save dir
	std::string saveDir = getExecutableDirectory().generic_string() + "/data/savedata.txt";

	// Try save file to local dir
	std::ofstream saveFile(saveDir);

	if (saveFile.is_open())
	{
		std::cout << "Saved file!\n";
		saveFile << maximized;
	}
}

// Get the parent path of the programs executable
std::filesystem::path getExecutableDirectory()
{
	char filePathName[1024];
	std::filesystem::path exePath;

	// Get filepath as char, then set filePathName using return
	GetModuleFileNameA(nullptr, filePathName, 1024);

	// Convert filePathName to a filesystem::path 
	exePath = filePathName;

	// Return parent path (filepath - file name)
	return exePath.parent_path();
}

void tryLogError()
{
	SDL_Log(SDL_GetError());
}