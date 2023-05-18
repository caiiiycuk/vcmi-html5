/*
 * CIntObject.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../render/Graphics.h"
#include "../../lib/Rect.h"
#include "EventsReceiver.h"

struct SDL_Surface;
class CGuiHandler;
class CPicture;

class IUpdateable
{
public:
	virtual void update()=0;
	virtual ~IUpdateable() = default;
};

class IShowActivatable
{
public:
	virtual void activate()=0;
	virtual void deactivate()=0;

	virtual void redraw()=0;
	virtual void show(SDL_Surface * to) = 0;
	virtual void showAll(SDL_Surface * to) = 0;

	virtual void onScreenResize() = 0;
	virtual ~IShowActivatable() = default;
};

// Base UI element
class CIntObject : public IShowActivatable, public AEventsReceiver //interface object
{
	ui16 used;

	//non-const versions of fields to allow changing them in CIntObject
	CIntObject *parent_m; //parent object

public:
	//redraw parent flag - this int may be semi-transparent and require redraw of parent window
	enum {REDRAW_PARENT=8};
	int type; //bin flags using etype

	std::vector<CIntObject *> children;

	/// read-only parent access. May not be a "clean" solution but allows some compatibility
	CIntObject * const & parent;

	/// position of object on the screen. Please do not modify this anywhere but in constructor - use moveBy\moveTo instead
	/*const*/ Rect pos;

	CIntObject(int used=0, Point offset=Point());
	virtual ~CIntObject();

	//hover handling
	void hover (bool on) override{}

	//keyboard handling
	bool captureAllKeys; //if true, only this object should get info about pressed keys

	bool captureThisKey(EShortcut key) override; //allows refining captureAllKeys against specific events (eg. don't capture ENTER)

	void addUsedEvents(ui16 newActions);
	void removeUsedEvents(ui16 newActions);

	enum {ACTIVATE=1, DEACTIVATE=2, UPDATE=4, SHOWALL=8, DISPOSE=16, SHARE_POS=32};
	ui8 defActions; //which calls will be tried to be redirected to children
	ui8 recActions; //which calls we allow to receive from parent

	/// deactivates if needed, blocks all automatic activity, allows only disposal
	void disable();
	/// activates if needed, all activity enabled (Warning: may not be symetric with disable if recActions was limited!)
	void enable();
	/// deactivates or activates UI element based on flag
	void setEnabled(bool on);

	// activate or deactivate object. Inactive object won't receive any input events (keyboard\mouse)
	// usually used automatically by parent
	void activate() override;
	void deactivate() override;

	//called each frame to update screen
	void show(SDL_Surface * to) override;
	//called on complete redraw only
	void showAll(SDL_Surface * to) override;
	//request complete redraw of this object
	void redraw() override;

	/// called only for windows whenever screen size changes
	/// default behavior is to re-center, can be overriden
	void onScreenResize() override;

	bool isInside(const Point & position) override;

	const Rect & center(const Rect &r, bool propagate = true); //sets pos so that r will be in the center of screen, assigns sizes of r to pos, returns new position
	const Rect & center(const Point &p, bool propagate = true);  //moves object so that point p will be in its center
	const Rect & center(bool propagate = true); //centers when pos.w and pos.h are set, returns new position
	void fitToScreen(int borderWidth, bool propagate = true); //moves window to fit into screen
	void moveBy(const Point &p, bool propagate = true);
	void moveTo(const Point &p, bool propagate = true);//move this to new position, coordinates are absolute (0,0 is topleft screen corner)

	void addChild(CIntObject *child, bool adjustPosition = false);
	void removeChild(CIntObject *child, bool adjustPosition = false);

	/// functions for printing text.
	/// Deprecated. Use CLabel where possible instead
	void printAtMiddleLoc(const std::string & text, const Point &p, EFonts font, SDL_Color color, SDL_Surface * dst);
	void printAtMiddleWBLoc(const std::string & text, const Point &p, EFonts font, int charsPerLine, SDL_Color color, SDL_Surface * dst);
};

/// Class for binding keys to left mouse button clicks
/// Classes wanting use it should have it as one of their base classes
class CKeyShortcut : public virtual CIntObject
{
	bool shortcutPressed;
public:
	EShortcut assignedKey;
	CKeyShortcut();
	CKeyShortcut(EShortcut key);
	void keyPressed(EShortcut key) override;
	void keyReleased(EShortcut key) override;

};

class WindowBase : public CIntObject
{
public:
	WindowBase(int used_ = 0, Point pos_ = Point());
protected:
	void close();
};

class IStatusBar
{
public:
	virtual ~IStatusBar();

	/// set current text for the status bar
	virtual void write(const std::string & text) = 0;

	/// remove any current text from the status bar
	virtual void clear() = 0;

	/// remove text from status bar if current text matches tested text
	virtual void clearIfMatching(const std::string & testedText) = 0;

	/// enables mode for entering text instead of showing hover text
	virtual void setEnteringMode(bool on) = 0;

	/// overrides hover text from controls with text entered into in-game console (for chat/cheats)
	virtual void setEnteredText(const std::string & text) = 0;

};

class EmptyStatusBar : public IStatusBar
{
	virtual void write(const std::string & text){};
	virtual void clear(){};
	virtual void clearIfMatching(const std::string & testedText){};
	virtual void setEnteringMode(bool on){};
	virtual void setEnteredText(const std::string & text){};
};
