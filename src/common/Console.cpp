/////////////////////////////////////////
//
//         OpenLieroX
//
// based on sources for Carnage Marines
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Console source code
// Created 7/4/02
// Jason Boettcher

#include "AuxLib.h"
#include "LieroX.h"
#include "console.h"
#include "GfxPrimitives.h"
#include "InputEvents.h"
#include "StringUtils.h"
#include "Options.h"

console_t	*Console = NULL;


///////////////////
// Initialize the console
int Con_Initialize(void)
{
	int n;
	Console = new console_t;;
	if(Console == NULL)
		return false;

	Console->fPosition = 1.0f;
	Console->iState = CON_HIDDEN;
	Console->iLastchar = 0;
	Console->icurHistory = -1;
	Console->iNumHistory = 0;
	Console->iCurpos = 0;
	Console->fBlinkTime = 0;
	Console->iBlinkState = 1;

	Console->fLastRepeat = -9999;
	Console->fTimePushed = -9999;
	Console->bHolding = false;

	for(n=0;n<MAX_CONLINES;n++) {
		Console->Line[n].strText = "";
		Console->Line[n].Colour = CNC_NORMAL;
	}

	for(n=0;n<MAX_CONHISTORY;n++)
		Console->History[n].strText = "";

    Console->bmpConPic = LoadImage("data/gfx/console.png");
    if(!Console->bmpConPic)
        return false;

	return true;
}


///////////////////
// Toggle the console
void Con_Toggle(void)
{
	// Update the cursor blink state
	Console->fBlinkTime = 0;
	Console->iBlinkState = 1;

	if(Console->iState == CON_HIDDEN || Console->iState == CON_HIDING) {
		Console->iState = CON_DROPPING;
        if(!tLXOptions->iFullscreen)
		    SDL_ShowCursor(SDL_ENABLE);
	}

	else if(Console->iState == CON_DROPPING || Console->iState == CON_DOWN) {
		Console->iState = CON_HIDING;
        if(!tLXOptions->iFullscreen)
		    SDL_ShowCursor(SDL_DISABLE);
	}
}


///////////////////
// Hide the console
void Con_Hide(void)
{
	Console->iCurpos = 0;
	Console->iState = CON_HIDDEN;
	Console->fPosition = 1;
	Console->fBlinkTime = 0;
	Console->iBlinkState = 1;
}


///////////////////
// Process the console
void Con_Process(float dt)
{
	keyboard_t *kb = GetKeyboard();
	SDL_Event *Ev = GetEvent();

	if(kb->KeyUp[SDLK_BACKQUOTE] || kb->KeyUp[SDLK_F1])
		Con_Toggle();

	if(Console->iState == CON_DROPPING)
		Console->fPosition -= 3.0f*dt;
	if(Console->iState == CON_HIDING)
		Console->fPosition += 3.0f*dt;

	if(Console->fPosition < 0.0f) {
		Console->iState = CON_DOWN;
		Console->fPosition = 0.0f;
	}
	if(Console->fPosition > 1) {
		Console->iState = CON_HIDDEN;
		Console->fPosition = 1;

		Console->Line[0].strText = "";
	}

	if(Console->iState != CON_DOWN && Console->iState != CON_DROPPING)
		return;


	// Add text to the console
	// TODO: why is this commented?
	//ProcessEvents();
	//SDL_Event *Ev = GetEvent();

	// Make sure a key event happened
	if(Ev->type != SDL_KEYUP && Ev->type != SDL_KEYDOWN)
		return;

	UnicodeChar input = 0;

	// Check the characters
	if(Ev->key.state == SDL_PRESSED) {
		input = Ev->key.keysym.unicode;

		// TODO: why is this so complicated? isn't there an easier method?
		// and why are there no comments on this code?
		if (input == 0)  {
			switch (Ev->key.keysym.sym) {
					case SDLK_LEFT:
					case SDLK_RIGHT:
					case SDLK_HOME:
					case SDLK_END:
					case SDLK_DELETE:
						input = Ev->key.keysym.sym;
						break;
					case SDLK_KP0:
					case SDLK_KP1:
					case SDLK_KP2:
					case SDLK_KP3:
					case SDLK_KP4:
					case SDLK_KP5:
					case SDLK_KP6:
					case SDLK_KP7:
					case SDLK_KP8:
					case SDLK_KP9:
					case SDLK_KP_MULTIPLY:
					case SDLK_KP_MINUS:
					case SDLK_KP_PLUS:
					case SDLK_KP_EQUALS:
						input = (uchar) (Ev->key.keysym.sym - 208);
						break;
					case SDLK_KP_PERIOD:
					case SDLK_KP_DIVIDE:
						input = (uchar) (Ev->key.keysym.sym - 220);
						break;
					case SDLK_KP_ENTER:
						input = '\r';
						break;
                    default:
                        break;
			}  // switch
		}

		// Process the input
		Con_ProcessCharacter(input);
	}

	// Handle more keys at once keydown
	for(int i=0; i<kb->queueLength; i++)
		if (kb->keyQueue[i].down && kb->keyQueue[i].ch != input)
			Con_ProcessCharacter(kb->keyQueue[i].ch);

	// Key up
	if(Ev->key.state == SDL_RELEASED && Ev->type == SDL_KEYUP)  {
		Console->iLastchar = '\0';
		Console->bHolding = false;
		Console->fTimePushed = -9999;
		Console->fLastRepeat = -9999;
	}

	// Handle the history keys

	// Up arrow
	if(kb->KeyUp[SDLK_UP]) {
		Console->icurHistory++;
		Console->icurHistory = MIN(Console->icurHistory,Console->iNumHistory-1);

		if(Console->icurHistory >= 0) {
			Console->Line[0].Colour = CNC_NORMAL;
			Console->Line[0].strText =  Console->History[Console->icurHistory].strText;
			Console->iCurpos = Console->Line[0].strText.size();
		}
	}

	// Down arrow
	if(kb->KeyUp[SDLK_DOWN]) {
		Console->icurHistory--;
		if(Console->icurHistory >= 0) {
			Console->Line[0].Colour = CNC_NORMAL;
			Console->Line[0].strText = Console->History[Console->icurHistory].strText;
		} else {
			Console->Line[0].strText = "";
		}

		Console->icurHistory = MAX(Console->icurHistory,-1);
	}
}


///////////////////
// Handles the character typed in the console
void Con_ProcessCharacter(UnicodeChar input)
{
	if (!input)
		return;

	// Key repeat handling
	if (Console->bHolding)  {
		if (Console->iLastchar != input)
			Console->bHolding = false;
		else  {
			if (tLX->fCurTime - Console->fTimePushed < 0.25f)
				return;
			if (tLX->fCurTime - Console->fLastRepeat < 0.03f)
				return;
			Console->fLastRepeat = tLX->fCurTime;
		}
	}

	if (!Console->bHolding)  {
		Console->bHolding = true;
		Console->fTimePushed = tLX->fCurTime;
	}


	// Handle the character
	Console->iBlinkState = 1;
	Console->fBlinkTime = 0;
	Console->iLastchar = input;


	// Backspace
	if(input == '\b') {
		if(Console->iCurpos > 0)  {
			Utf8Erase(Console->Line[0].strText, --Console->iCurpos, 1);
		}
		Console->icurHistory = -1;
		return;
	}

	// Delete
	if(input == SDLK_DELETE)  {
		if(Utf8StringSize(Console->Line[0].strText) > 0 && Utf8StringSize(Console->Line[0].strText) > Console->iCurpos)  {
			Utf8Erase(Console->Line[0].strText, Console->iCurpos, 1);
		}
		Console->icurHistory = -1;
		return;
	}

	// Left arrow
	if(input == SDLK_LEFT)  {
		if(Console->iCurpos > 0)
			Console->iCurpos--; // TODO utf
		return;
	}

	// Right arrow
	if(input == SDLK_RIGHT)  {
		if(Console->iCurpos < Utf8StringSize(Console->Line[0].strText))
			Console->iCurpos++;
		return;
	}

	// Home
	if(input == SDLK_HOME)  {
		Console->iCurpos = 0;
		return;
	}

	// End
	if(input == SDLK_END)  {
		Console->iCurpos = Utf8StringSize(Console->Line[0].strText);
		return;
	}

	// Paste
	if(input == 22)  {
		// Safety
		if (Console->iCurpos > Utf8StringSize(Console->Line[0].strText))
			Console->iCurpos = Utf8StringSize(Console->Line[0].strText);

		// Get the text
		std::string buf;
		buf = GetClipboardText();

		// Paste
		Console->Line[0].Colour = CNC_NORMAL;
		Utf8Insert(Console->Line[0].strText, Console->iCurpos, buf);
		Console->iCurpos += Utf8StringSize(buf);
		Console->icurHistory = -1;

		return;
	}


	// Enter key
	if(input == '\n' || input == '\r') {

		Con_Printf(CNC_NORMAL, "]" + Console->Line[0].strText);

		// Parse the line
		Cmd_ParseLine(Console->Line[0].strText);
		Con_AddHistory(Console->Line[0].strText);


		Console->Line[0].strText = "";
		Console->iCurpos = 0;

		return;
	}

	// Tab
	if(input == '\t') {
		// Auto-complete
		Cmd_AutoComplete(Console->Line[0].strText);
		Console->iCurpos = Utf8StringSize(Console->Line[0].strText);
		Console->icurHistory = -1;
		return;
	}

	// Normal key
	if(input > 31) {
		// Safety
		if (Console->iCurpos > Utf8StringSize(Console->Line[0].strText))
			Console->iCurpos = Utf8StringSize(Console->Line[0].strText);

		Console->Line[0].Colour = CNC_NORMAL;
		InsertUnicodeChar(Console->Line[0].strText, Console->iCurpos++, input);
		Console->icurHistory = -1;
	}
}


void Con_Printf(int color, const std::string& txt) {
	Con_AddText(color, txt);
}

///////////////////
// Add a string of text to the console
void Con_AddText(int colour, const std::string& text)
{
	if (text == "")
		return;

	const std::vector<std::string>& lines = explode(text,"\n");

	// Move all the text up, losing the last line
	int n;
	for(n=MAX_CONLINES-lines.size()-1;n>=1;n--) {
		// TODO: this seems wrong
		Console->Line[n+lines.size()].strText = Console->Line[n].strText;
		Console->Line[n+lines.size()].Colour = Console->Line[n].Colour;
	}

	// Add the lines
	n=1;
	for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++, n++)  {
		Console->Line[n].strText = *it;
		Console->Line[n].Colour = colour;
	}
}


///////////////////
// Add a command to the history
void Con_AddHistory(const std::string& text)
{
	// Move the history up one, dropping the last
	for(int n = MAX_CONHISTORY - 2; n >= 0; n--)
		Console->History[n+1].strText = Console->History[n].strText;

	Console->icurHistory = -1;
	Console->iNumHistory++;
	Console->iNumHistory = MIN(Console->iNumHistory, MAX_CONHISTORY - 1);

	Console->History[0].strText = text;
}


///////////////////
// Draw the console
void Con_Draw(SDL_Surface *bmpDest)
{
	if(Console->iState == CON_HIDDEN)
		return;

	int y = (int)(-Console->fPosition * (float)Console->bmpConPic->h);
	int texty = y+Console->bmpConPic->h-28;
	static std::string buf;

	const Uint32 Colours[6] = {tLX->clConsoleNormal, tLX->clConsoleNotify, tLX->clConsoleError, tLX->clConsoleWarning,
		                 tLX->clConsoleDev, tLX->clConsoleChat };

	DrawImage(bmpDest,Console->bmpConPic,0,y);


	// Draw the lines of text
	for(int n = 0; n < MAX_CONLINES; n++, texty -= 15) {
		buf = "";


		if(n==0)
			buf = "]";
		buf += Console->Line[n].strText;

		Console->fBlinkTime += tLX->fDeltaTime;
		if (Console->fBlinkTime > 10) {
			Console->iBlinkState = !Console->iBlinkState;
			Console->fBlinkTime = 0;
		}
		if(n==0 && Console->iBlinkState)  {
			DrawVLine(
				bmpDest,
				texty, texty + tLX->cFont.GetHeight(),
				17 + tLX->cFont.GetWidth(
					Utf8SubStr(Console->Line[n].strText, 0, Console->iCurpos)),
				tLX->clConsoleCursor);
		}

		tLX->cFont.Draw(bmpDest, 12, texty, Colours[Console->Line[n].Colour], buf);
	}
}


///////////////////
// Returns if the console is in use
bool Con_IsUsed(void)
{
	return Console->iState != CON_HIDDEN;
}


///////////////////
// Shutdown the console
void Con_Shutdown(void)
{
	if(Console)
		delete Console;

	Console = NULL;
}
