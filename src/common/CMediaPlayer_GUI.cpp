// OpenLieroX Media Player Interface Components
// Made by Dark Charlie and Albert Zeyer
// code under LGPL

#ifdef WITH_MEDIAPLAYER

#include "LieroX.h"
#include "AuxLib.h"
#include "Menu.h"
#include "Graphics.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "CMediaPlayer.h"
#include "CButton.h"
#include "CLabel.h"
#include "CCheckbox.h"
#include "Cursor.h"


//
// Player button
//

////////////////////
// Constructor
CPlayerButton::CPlayerButton(const SmartPointer<SDL_Surface> & image) {
	if (!image.get())
		return;

	bmpImage = image;
	bDown = false;
}

/////////////////////
// Draws the button
void CPlayerButton::Draw(SDL_Surface * bmpDest)
{
	int src_y = 0;
	if (bDown)
		src_y = bmpImage.get()->h/2;
	DrawImageAdv(bmpDest,bmpImage,0,src_y,iX,iY,bmpImage.get()->w,bmpImage.get()->h/2);
}

//////////////////////
// Mouse down on Player buton
int CPlayerButton::MouseDown(mouse_t *tMouse, int nDown)
{
	bDown = InBox(tMouse->X,tMouse->Y) != 0;
	return MP_WID_MOUSEDOWN;
}

///////////////////////
// Click on Player button
int CPlayerButton::MouseUp(mouse_t *tMouse, int nDown)
{
	bDown = false;
	return MP_BTN_CLICK;
}

//
// Player slider
//

///////////////////
// Constructor
CPlayerSlider::CPlayerSlider(const SmartPointer<SDL_Surface> & progress, const SmartPointer<SDL_Surface> & start, const SmartPointer<SDL_Surface> & end, const SmartPointer<SDL_Surface> & background, int max)  {
	if (!progress.get() || !start.get() || !end.get() || !background.get())
		return;
	iValue = 0;
	iMax = max;

	bmpProgress = progress;
	bmpStart = start;
	bmpEnd = end;
	bmpBackground = background;
}

////////////////////
// Draw the slider
void CPlayerSlider::Draw(SDL_Surface * bmpDest)
{
	// Background
	DrawImage(bmpDest,bmpBackground,iX,iY);

	// Get the width
	int x = iX+5+bmpStart.get()->w;
	int w = bmpBackground.get()->w - 5;
	int val = (int)( ((float)w/(float)iMax) * (float)iValue ) + x-bmpStart.get()->w;

	// Progress start
	if (((float)iValue/(float)iMax) > 0.02)  {
		DrawImage(bmpDest,bmpStart,iX+5,iY+3);
	}

	// Progress
	int max = MIN(val,bmpBackground.get()->w-10-bmpEnd.get()->w+x);
	for (int i=x;i<max;i+=bmpProgress.get()->w)  {
		DrawImage(bmpDest,bmpProgress,i,iY+3);
	}

	// Progress end
	if (((float)iValue/(float)iMax) >= 0.98)  {
		DrawImage(bmpDest,bmpEnd,iX+iWidth-5,iY+3);
	}
}

///////////////////////
// Mouse down event on the slider
int CPlayerSlider::MouseDown(mouse_t *tMouse, int nDown)
{
	bCanLoseFocus = false;

	int x = iX+5;
	int w = iWidth - 10;

	// Find the value
	int val = (int)( (float)iMax / ( (float)w / (float)(tMouse->X-x)) );
	iValue = val;

	// Clamp it
	if(tMouse->X > x+w)
		iValue = iMax;
	if(tMouse->X < x)
		iValue = 0;

	// Clamp the value
	iValue = MAX(0,iValue);
	iValue = MIN(iMax,iValue);

	return MP_SLD_CHANGE;
}

//
// Player toggle button
//

/////////////////////////
// Draws the toggle button
void CPlayerToggleBtn::Draw(SDL_Surface * bmpDest)
{
	int src_y = 0;
	if (bEnabled)
		src_y = bmpImage.get()->h/2;
	DrawImageAdv(bmpDest,bmpImage,0,src_y,iX,iY,bmpImage.get()->w,bmpImage.get()->h/2);
}

////////////////////////
// Click on the toggle button
int CPlayerToggleBtn::MouseUp(mouse_t *tMouse,int nDown)
{
	bEnabled = !bEnabled;
	return MP_TOG_TOGGLE;
}

//
// Player marquee
//


/////////////////////////
// Constructor
CPlayerMarquee::CPlayerMarquee(const std::string& text, Uint32 col)  {
	szText = text;
	fTime = 0;
	fEndWait = 0;
	iFrame = 0;
	iColour = col;
	iDirection = 1;
	iTextWidth = tLX->cFont.GetWidth(szText);
}

/////////////////////
// Draws the marquee
void CPlayerMarquee::Draw(SDL_Surface * bmpDest)
{
	if (iTextWidth <= iWidth)  {
		int x = iX + iWidth/2 - iTextWidth/2;
		tLX->cFont.Draw(bmpDest,x,iY,iColour,szText);
		return;
	}

	fTime += tLX->fDeltaTime;
	// Move the text
	if (fTime > MARQUEE_TIME)  {
		fTime = 0;
		iFrame += MARQUEE_STEP*iDirection;
		if (iFrame+iWidth >= iTextWidth+4)  { // +4 - let some space behind, looks better
			fEndWait += tLX->fDeltaTime;
			if (fEndWait >= MARQUEE_ENDWAIT)  {
				iDirection = -1;
				fEndWait = 0;
				iFrame += MARQUEE_STEP*iDirection;  // Move
			// Don't move at the end
			} else  {
				iFrame -= MARQUEE_STEP*iDirection;
			}
		}
		else if (iFrame <= 0 && iDirection == -1)  {
			fEndWait += tLX->fDeltaTime;
			if (fEndWait >= MARQUEE_ENDWAIT)  {
				iFrame = 0;
				iDirection = 1;
				fEndWait = 0;
				iFrame += MARQUEE_STEP*iDirection;  // Move
			// Don't move at the end
			} else {
				iFrame = 0;
			}
		}
	}

	// Draw the font passed into the widget rect
	tLX->cFont.DrawInRect(bmpDest,iX-iFrame,iY,iX,iY,iWidth,iHeight,iColour,szText);
}

//
//	Open/add directory dialog
//

enum  {
	od_List=0,
	od_Ok,
	od_Cancel,
	od_Add,
	od_IncludeSubdirs
};

////////////////////////
// Runs the dialog, returns the directory user selected
std::string COpenAddDir::Execute(const std::string& default_dir)
{
	szDir = default_dir;

	bool done = false;
	bool cancelled = false;

	keyboard_t *Keyboard = GetKeyboard();

	// Initialize the GUI
	cOpenGui.Initialize();
	cOpenGui.Add(new CListview(),od_List,iX+5,iY+45,iWidth-10,iHeight-115);
	cOpenGui.Add(new CLabel("Add to playlist",tLX->clNormalLabel),-1,iX+iWidth/2-tLX->cFont.GetWidth("Add to playlist")/2,iY+5,0,0);
	cOpenGui.Add(new CLabel("Select the folder by single-clicking on it",tLX->clNormalLabel),-1,iX+5,iY+25,0,0);
	cOpenGui.Add(new CCheckbox(bAdd),od_Add,iX+5,iY+iHeight-64,17,17);
	cOpenGui.Add(new CLabel("Add to current playlist",tLX->clNormalLabel),-1,iX+25,iY+iHeight-64,0,0);
	cOpenGui.Add(new CCheckbox(bIncludeSubdirs),od_IncludeSubdirs,iX+5,iY+iHeight-45,17,17);
	cOpenGui.Add(new CLabel("Include subdirectories",tLX->clNormalLabel),-1,iX+25,iY+iHeight-45,0,0);
	cOpenGui.Add(new CButton(BUT_CANCEL,tMenu->bmpButtons),od_Cancel,iX+iWidth/2,iY+iHeight-25,60,15);
	cOpenGui.Add(new CButton(BUT_OK,tMenu->bmpButtons),od_Ok,iX+iWidth/2-50,iY+iHeight-25,30,15);
	cOpenGui.SetGlobalProperty(PRP_REDRAWMENU, 0);

	((CButton *)(cOpenGui.getWidget(od_Ok)))->setRedrawMenu(false);
	((CButton *)(cOpenGui.getWidget(od_Cancel)))->setRedrawMenu(false);

	gui_event_t *ev = NULL;
	CListview *lv = (CListview *)cOpenGui.getWidget(od_List);
	if (!lv)
		return NULL;

	CCheckbox *c = NULL;

	lv->setRedrawMenu(false);

	// Fill the directory list with the default directory
	ReFillList(lv,default_dir);

	// Save the area we are going to draw on in a buffer
	SmartPointer<SDL_Surface> bmpBuffer = gfxCreateSurface(Screen->w,Screen->h);
	if (!bmpBuffer.get())
		return NULL;

	DrawImage(bmpBuffer.get(),Screen,0,0);
	float oldtime = GetMilliSeconds()-tLX->fDeltaTime;

	while (!done)  {
		oldtime = tLX->fCurTime;
		tLX->fCurTime = GetMilliSeconds();
		tLX->fDeltaTime = tLX->fCurTime-oldtime;

		ProcessEvents();

		// Restore the original screen before drawing
		DrawImage(Screen,bmpBuffer,0,0);

		// Background
		DrawRectFill(Screen,iX,iY,iX+iWidth,iY+iHeight,tLX->clBlack);

		// Title bar
		DrawRectFill(Screen,iX,iY,iX+iWidth,iY+22,MakeColour(0,0,64));

		// Border
		Menu_DrawBox(Screen,iX,iY,iX+iWidth,iY+iHeight);

		cOpenGui.Draw(Screen);
		ev = cOpenGui.Process();
		if (ev)  {
			switch(ev->iControlID)  {
			// Ok
			case od_Ok:
				if(ev->iEventMsg == BTN_MOUSEUP) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					if (lv->getCurSIndex().size() == 0)
						break;

					// Copy the directory
					szDir = GetAbsolutePath(lv->getCurSIndex());

					// We're done
					done = true;
					cancelled = false;
				}
				break;

			// Cancel
			case od_Cancel:
				if(ev->iEventMsg == BTN_MOUSEUP) {
					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					done = true;
					cancelled = true;
				}
				break;


			// Player list
			case od_List:
				if(ev->iEventMsg == LV_DOUBLECLK || ev->iEventMsg == LV_ENTER) {
					// Re-fill the list with the double-clicked directory
					ReFillList(lv,lv->getCurSIndex());
				}
				break;

			// Include subdirectories
			case od_IncludeSubdirs:
				if(ev->iEventMsg == CHK_CHANGED) {
					c = (CCheckbox *)cOpenGui.getWidget(od_IncludeSubdirs);
					bIncludeSubdirs = c->getValue() != 0;
				}
				break;

			// Add to playlist
			case od_Add:
				if(ev->iEventMsg == CHK_CHANGED) {
					c = (CCheckbox *)cOpenGui.getWidget(od_Add);
					bAdd = c->getValue() != 0;
				}
				break;
			}
		}

		// Handle the keyboard
		// TODO: make this event-based (don't check GetKeyboard() directly)
		if (Keyboard->KeyDown[SDLK_ESCAPE])  {
			done = true;
			cancelled = true;
		}

		// Should we quit?
		if (tLX->bQuitGame)  {
			done = true;
			cancelled = true;
		}

		// Draw the mouse
		SetGameCursor(CURSOR_ARROW);
		DrawCursor(GetVideoSurface());
		FlipScreen(GetVideoSurface());
		CapFPS();
	}

	// Restore and free the buffer
	DrawImage(Screen,bmpBuffer,0,0);
	bmpBuffer = NULL;

	// Free the GUI
	cOpenGui.Shutdown();

	if (cancelled)
		return "";
	else  {
		// Adjust
		replace(szDir,"//","/",szDir);
		replace(szDir,"\\/","/",szDir);
		return szDir;
	}
}

////////////////////
// Checks if the given directory is the root directory
bool COpenAddDir::IsRoot(const std::string& dir)
{
	if (dir.size() == 0)
		return true;

#ifdef WIN32
	if (dir.size() >= 6)
		return dir.substr(1, std::string::npos) == ":\\/..";
	else
		return false;
#else
	return GetAbsolutePath(dir) == "/";
#endif

}



		class addDirToList { public:
			CListview* lv;
			int* index;
			int* selected;
			const std::string& parent_dir;
			addDirToList(CListview* l, int* i, int* s, const std::string& pd) : lv(l), index(i), selected(s), parent_dir(pd) {}
			inline bool operator() (const std::string& directory) {
				// Extract the directory name from the path
				size_t dir_sep = findLastPathSep(directory);

				// Add the directory
				if (dir_sep != std::string::npos)  {
					if(parent_dir == directory.substr(dir_sep+1))
						*selected = *index;

					lv->AddItem(directory,(*index)++,tLX->clListView);
					lv->AddSubitem(LVS_TEXT, directory.substr(dir_sep+1), NULL, NULL);
				}

				return true;
			}
		};

///////////////////////
// Fills the list with the subdirectories of the "dir"
void COpenAddDir::ReFillList(CListview *lv, const std::string& dir)
{
	std::string absolute_path = GetAbsolutePath(dir);

	// Clear the listview
	lv->Clear();

	// If the target is root directory (i.e. C:\ on windows or / on unix)
	if (IsRoot(dir))  {
		int index = 0;
		drive_list drives = GetDrives();
		char cur_drive = *absolute_path.begin();
		for (drive_list::const_iterator i=drives.begin(); i != drives.end(); i++)  {
#ifdef WIN32
			if (i->type == DRV_FIXED)  {
#endif
				lv->AddItem(i->name,index,tLX->clListView);
				lv->AddSubitem(LVS_TEXT, i->name, NULL, NULL);
				if (cur_drive == i->name.at(0))
					lv->setSelectedID(index);
				index++;
#ifdef WIN32
			}
#endif
		}

	// The directory list
	} else  {
		int index = 0;

		// Add the parent directory
		lv->AddItem(absolute_path + "/..", index++, tLX->clListView);
		lv->AddSubitem(LVS_TEXT, "..", NULL, NULL);

		int selected = 0;

		FindFiles(addDirToList(lv, &index, &selected, absolute_path), absolute_path, true, FM_DIR);
		if(selected) lv->setSelectedID(selected);
	}

}

#endif // WITH_MEDIAPLAYER
