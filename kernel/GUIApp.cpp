/*
Copyright (C) 2002-2007 The Pentagram team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "pent_include.h"

#include "GUIApp.h"

#include <SDL.h>
#include <SDL_keyboard.h>
//!! a lot of these includes are just for some hacks... clean up sometime
#include "Kernel.h"
#include "FileSystem.h"
#include "SettingManager.h"
#include "ConfigFileManager.h"
#include "ObjectManager.h"
#include "GameInfo.h"
#include "FontManager.h"
#include "MemoryManager.h"

#include "HIDManager.h"
#include "Joystick.h"

#include "RenderSurface.h"
#include "Texture.h"
#include "FixedWidthFont.h"
#include "PaletteManager.h"
#include "Palette.h"
#include "GameData.h"
#include "World.h"
#include "Direction.h"
#include "Game.h"
#include "getObject.h"

#include "SavegameWriter.h"
#include "Savegame.h"
#include <ctime>

#include "Gump.h"
#include "DesktopGump.h"
#include "ConsoleGump.h"
#include "GameMapGump.h"
#include "InverterGump.h"
#include "ScalerGump.h"
#include "FastAreaVisGump.h"
#include "MiniMapGump.h"
#include "QuitGump.h"
#include "MenuGump.h"
#include "PentagramMenuGump.h"

// For gump positioning... perhaps shouldn't do it this way....
#include "BarkGump.h"
#include "AskGump.h"
#include "ModalGump.h"
#include "MessageBoxGump.h"


#include "QuickAvatarMoverProcess.h"
#include "Actor.h"
#include "ActorAnimProcess.h"
#include "TargetedAnimProcess.h"
#include "u8intrinsics.h"
#include "remorseintrinsics.h"
#include "Egg.h"
#include "CurrentMap.h"
#include "InverterProcess.h"
#include "HealProcess.h"
#include "SchedulerProcess.h"

#include "EggHatcherProcess.h" // for a hack
#include "UCProcess.h" // more hacking
#include "GumpNotifyProcess.h" // guess
#include "ActorBarkNotifyProcess.h" // guess
#include "DelayProcess.h"
#include "AvatarGravityProcess.h"
#include "MissileProcess.h"
#include "TeleportToEggProcess.h"
#include "ItemFactory.h"
#include "PathfinderProcess.h"
#include "AvatarMoverProcess.h"
#include "ResurrectionProcess.h"
#include "SplitItemProcess.h"
#include "ClearFeignDeathProcess.h"
#include "LoiterProcess.h"
#include "AvatarDeathProcess.h"
#include "GrantPeaceProcess.h"
#include "CombatProcess.h"
#include "FireballProcess.h"
#include "DestroyItemProcess.h"
#include "AmbushProcess.h"
#include "Pathfinder.h"

#include "MovieGump.h"
#include "ShapeViewerGump.h"

#include "AudioMixer.h"

#ifdef WIN32
#include <windows.h>
#endif

#if defined(WIN32) && defined(I_AM_COLOURLESS_EXPERIMENTING_WITH_HW_CURSORS)
#include "Shape.h"
#include "ShapeFrame.h"
#include "SoftRenderSurface.h"
#include "SDL_syswm.h"

struct HWMouseCursor {
	HICON hCursor;
};

#endif

#include "XFormBlend.h"

#include "MusicProcess.h"
#include "AudioProcess.h"

#include "util.h"

using std::string;

DEFINE_RUNTIME_CLASSTYPE_CODE(GUIApp,CoreApp);

GUIApp::GUIApp(int argc, const char* const* argv)
	: CoreApp(argc, argv), save_count(0), game(0), kernel(0), objectmanager(0),
	  hidmanager(0), ucmachine(0), screen(0), fullscreen(false), palettemanager(0), 
	  gamedata(0), world(0), desktopGump(0), consoleGump(0), gameMapGump(0),
	  avatarMoverProcess(0), runSDLInit(false),
	  frameSkip(false), frameLimit(true), interpolate(true),
	  animationRate(100), avatarInStasis(false), paintEditorItems(false),
	  painting(false), showTouching(false), mouseX(0), mouseY(0),
	  defMouse(0), flashingcursor(0), 
	  mouseOverGump(0), dragging(DRAG_NOT), dragging_offsetX(0),
	  dragging_offsetY(0), inversion(0), timeOffset(0), has_cheated(false),
	  drawRenderStats(false), ttfoverrides(false), audiomixer(0)
{
	application = this;

	for (int i = 0; i < MOUSE_LAST; ++i) {
		mouseButton[i].downGump = 0;
		mouseButton[i].lastDown = 0;
		mouseButton[i].state = MBS_HANDLED;
	}

	for (uint16 key=0; key < HID_LAST; ++key)
	{
		lastDown[key] = 0;
		down[key] = 0;
	}

	con.AddConsoleCommand("quit", ConCmd_quit);
	con.AddConsoleCommand("GUIApp::quit", ConCmd_quit);
	con.AddConsoleCommand("QuitGump::verifyQuit", QuitGump::ConCmd_verifyQuit);
	con.AddConsoleCommand("ShapeViewerGump::U8ShapeViewer", ShapeViewerGump::ConCmd_U8ShapeViewer);
	con.AddConsoleCommand("MenuGump::showMenu", MenuGump::ConCmd_showMenu);
	con.AddConsoleCommand("GUIApp::drawRenderStats", ConCmd_drawRenderStats);
	con.AddConsoleCommand("GUIApp::engineStats", ConCmd_engineStats);

	con.AddConsoleCommand("GUIApp::changeGame",ConCmd_changeGame);
	con.AddConsoleCommand("GUIApp::listGames",ConCmd_listGames);

	con.AddConsoleCommand("GUIApp::memberVar", &GUIApp::ConCmd_memberVar);
	con.AddConsoleCommand("GUIApp::setVideoMode",ConCmd_setVideoMode);
	con.AddConsoleCommand("GUIApp::toggleFullscreen",ConCmd_toggleFullscreen);

	con.AddConsoleCommand("GUIApp::toggleAvatarInStasis",ConCmd_toggleAvatarInStasis);
	con.AddConsoleCommand("GUIApp::togglePaintEditorItems",ConCmd_togglePaintEditorItems);
	con.AddConsoleCommand("GUIApp::toggleShowTouchingItems",ConCmd_toggleShowTouchingItems);

	con.AddConsoleCommand("GUIApp::closeItemGumps",ConCmd_closeItemGumps);

	con.AddConsoleCommand("HIDManager::bind", HIDManager::ConCmd_bind);
	con.AddConsoleCommand("HIDManager::unbind", HIDManager::ConCmd_unbind);
	con.AddConsoleCommand("HIDManager::listbinds",
						  HIDManager::ConCmd_listbinds);
	con.AddConsoleCommand("HIDManager::save", HIDManager::ConCmd_save);
	con.AddConsoleCommand("Kernel::processTypes", Kernel::ConCmd_processTypes);
	con.AddConsoleCommand("Kernel::processInfo", Kernel::ConCmd_processInfo);
	con.AddConsoleCommand("Kernel::listProcesses",
						  Kernel::ConCmd_listProcesses);
	con.AddConsoleCommand("Kernel::toggleFrameByFrame",
						  Kernel::ConCmd_toggleFrameByFrame);
	con.AddConsoleCommand("Kernel::advanceFrame", Kernel::ConCmd_advanceFrame);
	con.AddConsoleCommand("ObjectManager::objectTypes",
						  ObjectManager::ConCmd_objectTypes);
	con.AddConsoleCommand("ObjectManager::objectInfo",
						  ObjectManager::ConCmd_objectInfo);
	con.AddConsoleCommand("MemoryManager::MemInfo",
						  MemoryManager::ConCmd_MemInfo);
	con.AddConsoleCommand("MemoryManager::test",
						  MemoryManager::ConCmd_test);

	con.AddConsoleCommand("QuickAvatarMoverProcess::startMoveUp",
						  QuickAvatarMoverProcess::ConCmd_startMoveUp);
	con.AddConsoleCommand("QuickAvatarMoverProcess::startMoveDown",
						  QuickAvatarMoverProcess::ConCmd_startMoveDown);
	con.AddConsoleCommand("QuickAvatarMoverProcess::startMoveLeft",
						  QuickAvatarMoverProcess::ConCmd_startMoveLeft);
	con.AddConsoleCommand("QuickAvatarMoverProcess::startMoveRight",
						  QuickAvatarMoverProcess::ConCmd_startMoveRight);
	con.AddConsoleCommand("QuickAvatarMoverProcess::startAscend",
						  QuickAvatarMoverProcess::ConCmd_startAscend);
	con.AddConsoleCommand("QuickAvatarMoverProcess::startDescend",
						  QuickAvatarMoverProcess::ConCmd_startDescend);
	con.AddConsoleCommand("QuickAvatarMoverProcess::stopMoveUp",
						  QuickAvatarMoverProcess::ConCmd_stopMoveUp);
	con.AddConsoleCommand("QuickAvatarMoverProcess::stopMoveDown",
						  QuickAvatarMoverProcess::ConCmd_stopMoveDown);
	con.AddConsoleCommand("QuickAvatarMoverProcess::stopMoveLeft",
						  QuickAvatarMoverProcess::ConCmd_stopMoveLeft);
	con.AddConsoleCommand("QuickAvatarMoverProcess::stopMoveRight",
						  QuickAvatarMoverProcess::ConCmd_stopMoveRight);
	con.AddConsoleCommand("QuickAvatarMoverProcess::stopAscend",
						  QuickAvatarMoverProcess::ConCmd_stopAscend);
	con.AddConsoleCommand("QuickAvatarMoverProcess::stopDescend",
						  QuickAvatarMoverProcess::ConCmd_stopDescend);
	con.AddConsoleCommand("QuickAvatarMoverProcess::toggleQuarterSpeed",
						  QuickAvatarMoverProcess::ConCmd_toggleQuarterSpeed);
	con.AddConsoleCommand("QuickAvatarMoverProcess::toggleClipping",
						  QuickAvatarMoverProcess::ConCmd_toggleClipping);

	con.AddConsoleCommand("GameMapGump::toggleHighlightItems",
						  GameMapGump::ConCmd_toggleHighlightItems);
	con.AddConsoleCommand("GameMapGump::dumpMap",
						  GameMapGump::ConCmd_dumpMap);
	con.AddConsoleCommand("GameMapGump::incrementSortOrder",
						  GameMapGump::ConCmd_incrementSortOrder);
	con.AddConsoleCommand("GameMapGump::decrementSortOrder",
						  GameMapGump::ConCmd_decrementSortOrder);

	con.AddConsoleCommand("AudioProcess::listSFX", AudioProcess::ConCmd_listSFX);
	con.AddConsoleCommand("AudioProcess::playSFX", AudioProcess::ConCmd_playSFX);
	con.AddConsoleCommand("AudioProcess::stopSFX", AudioProcess::ConCmd_stopSFX);

	// Game related console commands are now added in startupGame
}

GUIApp::~GUIApp()
{
	shutdown();

	con.RemoveConsoleCommand(GUIApp::ConCmd_quit);
	con.RemoveConsoleCommand(QuitGump::ConCmd_verifyQuit);
	con.RemoveConsoleCommand(ShapeViewerGump::ConCmd_U8ShapeViewer);
	con.RemoveConsoleCommand(MenuGump::ConCmd_showMenu);
	con.RemoveConsoleCommand(GUIApp::ConCmd_drawRenderStats);
	con.RemoveConsoleCommand(GUIApp::ConCmd_engineStats);

	con.RemoveConsoleCommand(GUIApp::ConCmd_changeGame);
	con.RemoveConsoleCommand(GUIApp::ConCmd_listGames);

	con.RemoveConsoleCommand(GUIApp::ConCmd_memberVar);
	con.RemoveConsoleCommand(GUIApp::ConCmd_setVideoMode);
	con.RemoveConsoleCommand(GUIApp::ConCmd_toggleFullscreen);

	con.RemoveConsoleCommand(GUIApp::ConCmd_toggleAvatarInStasis);
	con.RemoveConsoleCommand(GUIApp::ConCmd_togglePaintEditorItems);
	con.RemoveConsoleCommand(GUIApp::ConCmd_toggleShowTouchingItems);

	con.RemoveConsoleCommand(GUIApp::ConCmd_closeItemGumps);

	con.RemoveConsoleCommand(HIDManager::ConCmd_bind);
	con.RemoveConsoleCommand(HIDManager::ConCmd_unbind);
	con.RemoveConsoleCommand(HIDManager::ConCmd_listbinds);
	con.RemoveConsoleCommand(HIDManager::ConCmd_save);
	con.RemoveConsoleCommand(Kernel::ConCmd_processTypes);
	con.RemoveConsoleCommand(Kernel::ConCmd_processInfo);
	con.RemoveConsoleCommand(Kernel::ConCmd_listProcesses);
	con.RemoveConsoleCommand(Kernel::ConCmd_toggleFrameByFrame);
	con.RemoveConsoleCommand(Kernel::ConCmd_advanceFrame);
	con.RemoveConsoleCommand(ObjectManager::ConCmd_objectTypes);
	con.RemoveConsoleCommand(ObjectManager::ConCmd_objectInfo);
	con.RemoveConsoleCommand(MemoryManager::ConCmd_MemInfo);
	con.RemoveConsoleCommand(MemoryManager::ConCmd_test);

	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_startMoveUp);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_startMoveDown);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_startMoveLeft);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_startMoveRight);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_startAscend);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_startDescend);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_stopMoveUp);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_stopMoveDown);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_stopMoveLeft);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_stopMoveRight);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_stopAscend);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_stopDescend);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_toggleQuarterSpeed);
	con.RemoveConsoleCommand(QuickAvatarMoverProcess::ConCmd_toggleClipping);

	con.RemoveConsoleCommand(GameMapGump::ConCmd_toggleHighlightItems);
	con.RemoveConsoleCommand(GameMapGump::ConCmd_dumpMap);
	con.RemoveConsoleCommand(GameMapGump::ConCmd_incrementSortOrder);
	con.RemoveConsoleCommand(GameMapGump::ConCmd_decrementSortOrder);

	con.RemoveConsoleCommand(AudioProcess::ConCmd_listSFX);
	con.RemoveConsoleCommand(AudioProcess::ConCmd_stopSFX);
	con.RemoveConsoleCommand(AudioProcess::ConCmd_playSFX);

	// Game related console commands are now removed in shutdownGame

	FORGET_OBJECT(kernel);
	FORGET_OBJECT(defMouse);
	FORGET_OBJECT(objectmanager);
	FORGET_OBJECT(hidmanager);
	FORGET_OBJECT(audiomixer);
	FORGET_OBJECT(ucmachine);
	FORGET_OBJECT(palettemanager);
	FORGET_OBJECT(gamedata);
	FORGET_OBJECT(world);
	FORGET_OBJECT(ucmachine);
	FORGET_OBJECT(fontmanager);
	FORGET_OBJECT(screen);
}

// Init sdl
void GUIApp::SDLInit()
{
	con.Print(MM_INFO, "Initialising SDL...\n");
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	atexit(SDL_Quit);
}

void GUIApp::startup()
{
	SDLInit();

	// Set the console to auto paint, till we have finished initing
	con.SetAutoPaint(conAutoPaint);

	pout << "-- Initializing Pentagram -- " << std::endl;

	// parent's startup first
	CoreApp::startup();

	bool dataoverride;
	if (!settingman->get("dataoverride", dataoverride,
						 SettingManager::DOM_GLOBAL))
		dataoverride = false;
	filesystem->initBuiltinData(dataoverride);

	kernel = new Kernel();

	//!! move this elsewhere
	kernel->addProcessLoader("DelayProcess",
							 ProcessLoader<DelayProcess>::load);
	kernel->addProcessLoader("GravityProcess",
							 ProcessLoader<GravityProcess>::load);
	kernel->addProcessLoader("AvatarGravityProcess",
							 ProcessLoader<AvatarGravityProcess>::load);
	kernel->addProcessLoader("PaletteFaderProcess",
							 ProcessLoader<PaletteFaderProcess>::load);
	kernel->addProcessLoader("TeleportToEggProcess",
							 ProcessLoader<TeleportToEggProcess>::load);
	kernel->addProcessLoader("ActorAnimProcess",
							 ProcessLoader<ActorAnimProcess>::load);
	kernel->addProcessLoader("TargetedAnimProcess",
							 ProcessLoader<TargetedAnimProcess>::load);
	kernel->addProcessLoader("AvatarMoverProcess",
							 ProcessLoader<AvatarMoverProcess>::load);
	kernel->addProcessLoader("QuickAvatarMoverProcess",
							 ProcessLoader<QuickAvatarMoverProcess>::load);
	kernel->addProcessLoader("PathfinderProcess",
							 ProcessLoader<PathfinderProcess>::load);
	kernel->addProcessLoader("SpriteProcess",
							 ProcessLoader<SpriteProcess>::load);
	kernel->addProcessLoader("MissileProcess",
							 ProcessLoader<MissileProcess>::load);
	kernel->addProcessLoader("CameraProcess",
							 ProcessLoader<CameraProcess>::load);
	kernel->addProcessLoader("MusicProcess",
							 ProcessLoader<MusicProcess>::load);
	kernel->addProcessLoader("AudioProcess",
							 ProcessLoader<AudioProcess>::load);
	kernel->addProcessLoader("EggHatcherProcess",
							 ProcessLoader<EggHatcherProcess>::load);
	kernel->addProcessLoader("UCProcess",
							 ProcessLoader<UCProcess>::load);
	kernel->addProcessLoader("GumpNotifyProcess",
							 ProcessLoader<GumpNotifyProcess>::load);
	kernel->addProcessLoader("ResurrectionProcess",
							 ProcessLoader<ResurrectionProcess>::load);
	kernel->addProcessLoader("DeleteActorProcess",
							 ProcessLoader<DestroyItemProcess>::load);	// YES, this is intentional
	kernel->addProcessLoader("DestroyItemProcess",
							 ProcessLoader<DestroyItemProcess>::load);
	kernel->addProcessLoader("SplitItemProcess",
							 ProcessLoader<SplitItemProcess>::load);
	kernel->addProcessLoader("ClearFeignDeathProcess",
							 ProcessLoader<ClearFeignDeathProcess>::load);
	kernel->addProcessLoader("LoiterProcess",
							 ProcessLoader<LoiterProcess>::load);
	kernel->addProcessLoader("AvatarDeathProcess",
							 ProcessLoader<AvatarDeathProcess>::load);
	kernel->addProcessLoader("GrantPeaceProcess",
							 ProcessLoader<GrantPeaceProcess>::load);
	kernel->addProcessLoader("CombatProcess",
							 ProcessLoader<CombatProcess>::load);
	kernel->addProcessLoader("FireballProcess",
							 ProcessLoader<FireballProcess>::load);
	kernel->addProcessLoader("HealProcess",
							 ProcessLoader<HealProcess>::load);
	kernel->addProcessLoader("SchedulerProcess",
							 ProcessLoader<SchedulerProcess>::load);
	kernel->addProcessLoader("InverterProcess",
							 ProcessLoader<InverterProcess>::load);
	kernel->addProcessLoader("ActorBarkNotifyProcess",
							 ProcessLoader<ActorBarkNotifyProcess>::load);
	kernel->addProcessLoader("JoystickCursorProcess",
							 ProcessLoader<JoystickCursorProcess>::load);
	kernel->addProcessLoader("AmbushProcess",
							 ProcessLoader<AmbushProcess>::load);

	objectmanager = new ObjectManager();

	GraphicSysInit();

	SDL_ShowCursor(SDL_DISABLE);
	SDL_GetMouseState(&mouseX, &mouseY);

	hidmanager = new HIDManager();

	// Audio Mixer
	audiomixer = new Pentagram::AudioMixer(22050,true,8);

	pout << "-- Pentagram Initialized -- " << std::endl << std::endl;

	// We Attempt to startup game
	setupGameList();
	GameInfo* info = getDefaultGame();
	if (setupGame(info))
		startupGame();
	else
		startupPentagramMenu();

	// Unset the console auto paint, since we have finished initing
	con.SetAutoPaint(0);
#ifdef LIBXENON
	con.unsetOutputEnabled(1);
#endif
//	pout << "Paint Initial display" << std::endl;
	paint();
}

void GUIApp::startupGame()
{
	con.SetAutoPaint(conAutoPaint);

	pout  << std::endl << "-- Initializing Game: " << gameinfo->name << " --" << std::endl;

	GraphicSysInit();

	// set window title to current game
	std::string title = "Pentagram - ";
	title += getGameInfo()->getGameTitle();
	SDL_WM_SetCaption(title.c_str(), "");

	// Generic Commands
	con.AddConsoleCommand("GUIApp::saveGame", ConCmd_saveGame);
	con.AddConsoleCommand("GUIApp::loadGame", ConCmd_loadGame);
	con.AddConsoleCommand("GUIApp::newGame", ConCmd_newGame);
#ifdef DEBUG
	con.AddConsoleCommand("Pathfinder::visualDebug",
						  Pathfinder::ConCmd_visualDebug);
#endif

	// U8 Game commands
	con.AddConsoleCommand("MainActor::teleport", MainActor::ConCmd_teleport);
	con.AddConsoleCommand("MainActor::mark", MainActor::ConCmd_mark);
	con.AddConsoleCommand("MainActor::recall", MainActor::ConCmd_recall);
	con.AddConsoleCommand("MainActor::listmarks", MainActor::ConCmd_listmarks);
	con.AddConsoleCommand("Cheat::maxstats", MainActor::ConCmd_maxstats);
	con.AddConsoleCommand("Cheat::heal", MainActor::ConCmd_heal);
	con.AddConsoleCommand("Cheat::toggleInvincibility", MainActor::ConCmd_toggleInvincibility);
	con.AddConsoleCommand("MainActor::name", MainActor::ConCmd_name);
	con.AddConsoleCommand("MovieGump::play", MovieGump::ConCmd_play);
	con.AddConsoleCommand("MusicProcess::playMusic", MusicProcess::ConCmd_playMusic);
	con.AddConsoleCommand("InverterProcess::invertScreen",
						  InverterProcess::ConCmd_invertScreen);
	con.AddConsoleCommand("FastAreaVisGump::toggle",
						  FastAreaVisGump::ConCmd_toggle);
	con.AddConsoleCommand("MiniMapGump::toggle",
						  MiniMapGump::ConCmd_toggle);
	con.AddConsoleCommand("MainActor::useBackpack",
						  MainActor::ConCmd_useBackpack);
	con.AddConsoleCommand("MainActor::useInventory",
						  MainActor::ConCmd_useInventory);
	con.AddConsoleCommand("MainActor::useRecall",
						  MainActor::ConCmd_useRecall);
	con.AddConsoleCommand("MainActor::useBedroll",
						  MainActor::ConCmd_useBedroll);
	con.AddConsoleCommand("MainActor::useKeyring",
						  MainActor::ConCmd_useKeyring);
	con.AddConsoleCommand("MainActor::toggleCombat",
						  MainActor::ConCmd_toggleCombat);

	gamedata = new GameData(gameinfo);

	std::string bindingsfile;
	if (GAME_IS_U8) {
		bindingsfile = "@data/u8bindings.ini";
	} else if (GAME_IS_REMORSE) {
		bindingsfile = "@data/remorsebindings.ini";
	}
	if (!bindingsfile.empty()) {
		// system-wide config
		if (configfileman->readConfigFile(bindingsfile,
										  "bindings", true))
			con.Printf(MM_INFO, "%s... Ok\n", bindingsfile.c_str());
		else
			con.Printf(MM_MINOR_WARN, "%s... Failed\n", bindingsfile.c_str());
	}

	hidmanager->loadBindings();
	
	if (GAME_IS_U8) {
		ucmachine = new UCMachine(U8Intrinsics, 256);
	} else if (GAME_IS_REMORSE) {
		ucmachine = new UCMachine(RemorseIntrinsics, 308);
	} else {
		CANT_HAPPEN_MSG("Invalid game type.");
	}

	inBetweenFrame = 0;
	lerpFactor = 256;

	// Initialize world
	world = new World();
	world->initMaps();

	game = Game::createGame(getGameInfo());

	settingman->setDefault("ttf", false);
	settingman->get("ttf", ttfoverrides);

	settingman->setDefault("frameSkip", false);
	settingman->get("frameSkip", frameSkip);

	settingman->setDefault("frameLimit", true);
	settingman->get("frameLimit", frameLimit);

	settingman->setDefault("interpolate", true);
	settingman->get("interpolate", interpolate);

	game->loadFiles();
	gamedata->setupFontOverrides();

	// Unset the console auto paint (can't have it from here on)
	con.SetAutoPaint(0);

	// Create Midi Driver for Ultima 8
	if (getGameInfo()->type == GameInfo::GAME_U8) 
		audiomixer->openMidiOutput();

	std::string savegame;
	settingman->setDefault("lastSave", "");
	settingman->get("lastSave", savegame);

	newGame(savegame);

	consoleGump->HideConsole();

	pout << "-- Game Initialized --" << std::endl << std::endl;
}

void GUIApp::startupPentagramMenu()
{
	con.SetAutoPaint(conAutoPaint);

	pout << std::endl << "-- Initializing Pentagram Menu -- " << std::endl;

	setupGame(getGameInfo("pentagram"));
	assert(gameinfo);

	GraphicSysInit();

	// Unset the console auto paint, since we have finished initing
	con.SetAutoPaint(0);
	consoleGump->HideConsole();

	Pentagram::Rect dims;
	desktopGump->GetDims(dims);

	Gump* menugump = new PentagramMenuGump(0,0,dims.w,dims.h);
	menugump->InitGump(0, true);
}

void GUIApp::shutdown()
{
	shutdownGame(false);
}

void GUIApp::shutdownGame(bool reloading)
{
	pout << "-- Shutting down Game -- " << std::endl;

	// Save config here....

	SDL_WM_SetCaption("Pentagram", "");

	textmodes.clear();

	// reset mouse cursor
	while (!cursors.empty()) cursors.pop();
	pushMouseCursor();

	if (audiomixer) {
		audiomixer->closeMidiOutput();
		audiomixer->reset();
	}

	FORGET_OBJECT(world);
	objectmanager->reset();
	FORGET_OBJECT(ucmachine);
	kernel->reset();
	palettemanager->reset();
	fontmanager->resetGameFonts();

	FORGET_OBJECT(game);
	FORGET_OBJECT(gamedata);

	desktopGump = 0;
	consoleGump = 0;
	gameMapGump = 0;
	scalerGump = 0;
	inverterGump = 0;

	timeOffset = -(sint32)Kernel::get_instance()->getFrameNum();
	inversion = 0;
	save_count = 0;
	has_cheated = false;

	// Generic Game 
	con.RemoveConsoleCommand(GUIApp::ConCmd_saveGame);
	con.RemoveConsoleCommand(GUIApp::ConCmd_loadGame);
	con.RemoveConsoleCommand(GUIApp::ConCmd_newGame);
#ifdef DEBUG
	con.RemoveConsoleCommand(Pathfinder::ConCmd_visualDebug);
#endif

	// U8 Only kind of
	con.RemoveConsoleCommand(MainActor::ConCmd_teleport);
	con.RemoveConsoleCommand(MainActor::ConCmd_mark);
	con.RemoveConsoleCommand(MainActor::ConCmd_recall);
	con.RemoveConsoleCommand(MainActor::ConCmd_listmarks);
	con.RemoveConsoleCommand(MainActor::ConCmd_maxstats);
	con.RemoveConsoleCommand(MainActor::ConCmd_heal);
	con.RemoveConsoleCommand(MainActor::ConCmd_toggleInvincibility);
	con.RemoveConsoleCommand(MainActor::ConCmd_name);
	con.RemoveConsoleCommand(MovieGump::ConCmd_play);
	con.RemoveConsoleCommand(MusicProcess::ConCmd_playMusic);
	con.RemoveConsoleCommand(InverterProcess::ConCmd_invertScreen);
	con.RemoveConsoleCommand(FastAreaVisGump::ConCmd_toggle);
	con.RemoveConsoleCommand(MiniMapGump::ConCmd_toggle);
	con.RemoveConsoleCommand(MainActor::ConCmd_useBackpack);
	con.RemoveConsoleCommand(MainActor::ConCmd_useInventory);
	con.RemoveConsoleCommand(MainActor::ConCmd_useRecall);
	con.RemoveConsoleCommand(MainActor::ConCmd_useBedroll);
	con.RemoveConsoleCommand(MainActor::ConCmd_useKeyring);
	con.RemoveConsoleCommand(MainActor::ConCmd_toggleCombat);

	// Kill Game
	CoreApp::killGame();

	pout << "-- Game Shutdown -- " << std::endl;

	if (reloading) {
		Pentagram::Rect dims;
		screen->GetSurfaceDims(dims);

		con.Print(MM_INFO, "Creating Desktop...\n");
		desktopGump = new DesktopGump(0,0, dims.w, dims.h);
		desktopGump->InitGump(0);
		desktopGump->MakeFocus();

		con.Print(MM_INFO, "Creating ScalerGump...\n");
		scalerGump = new ScalerGump(0, 0, dims.w, dims.h);
		scalerGump->InitGump(0);

		Pentagram::Rect scaled_dims;
		scalerGump->GetDims(scaled_dims);

		con.Print(MM_INFO, "Creating Graphics Console...\n");
		consoleGump = new ConsoleGump(0, 0, dims.w, dims.h);
		consoleGump->InitGump(0);
		consoleGump->HideConsole();
		
		con.Print(MM_INFO, "Creating Inverter...\n");
		inverterGump = new InverterGump(0, 0, scaled_dims.w, scaled_dims.h);
		inverterGump->InitGump(0);

		enterTextMode(consoleGump);
	}
}

void GUIApp::changeGame(Pentagram::istring newgame)
{
	change_gamename = newgame;
}

void GUIApp::menuInitMinimal(Pentagram::istring gamename)
{
	// Only if in the pentagram menu
	if (gameinfo->name != "pentagram") return;
	GameInfo *info = getGameInfo(gamename);
	if (!info) info = getGameInfo("pentagram");
	assert(info);

	pout  << std::endl << "-- Loading minimal game data for: " << info->name << " --" << std::endl;

	FORGET_OBJECT(game);
	FORGET_OBJECT(gamedata);


	setupGamePaths(info);

	if (info->name == "pentagram") return;

	gamedata = new GameData(info);
	game = Game::createGame(info);

	game->loadFiles();
	gamedata->setupFontOverrides();

	pout << "-- Finished loading minimal--" << std::endl << std::endl;
}

void GUIApp::DeclareArgs()
{
	// parent's arguments first
	CoreApp::DeclareArgs();

	// anything else?
}

void GUIApp::run()
{
	isRunning = true;

	sint32 next_ticks = SDL_GetTicks()*3;	// Next time is right now!
	
	SDL_Event event;
	while (isRunning) {
		inBetweenFrame = true;	// Will get set false if it's not an inBetweenFrame

		if (!frameLimit) {			
			kernel->runProcesses();
			desktopGump->run();
			inBetweenFrame = false;
			next_ticks = animationRate + SDL_GetTicks()*3;
			lerpFactor = 256;
		}
		else 
		{
			sint32 ticks = SDL_GetTicks()*3;
			sint32 diff = next_ticks - ticks;

			while (diff < 0) {
				next_ticks += animationRate;
				kernel->runProcesses();
				desktopGump->run();
#if 0
				perr << "--------------------------------------" << std::endl;
				perr << "NEW FRAME" << std::endl;
				perr << "--------------------------------------" << std::endl;
#endif
				inBetweenFrame = false;

				ticks = SDL_GetTicks()*3;

				// If frame skipping is off, we will only recalc next
				// ticks IF the frames are taking up 'way' too much time. 
				if (!frameSkip && diff <= -animationRate*2) next_ticks = animationRate + ticks;

				diff = next_ticks - ticks;
				if (!frameSkip) break;
			}

			// Calculate the lerp_factor
			lerpFactor = ((animationRate-diff)*256)/animationRate;
			//pout << "lerpFactor: " << lerpFactor << " framenum: " << framenum << std::endl;
			if (!interpolate || kernel->isPaused() || lerpFactor > 256)
				lerpFactor = 256;
		}

		// get & handle all events in queue
		while (isRunning && SDL_PollEvent(&event)) {
			handleEvent(event);
		}
		handleDelayedEvents();

		// Paint Screen
		paint();

		if (!change_gamename.empty()) {
			pout << "Changing Game to: " << change_gamename << std::endl;

			GameInfo* info = getGameInfo(change_gamename);

			if (info) {
				shutdownGame();

				change_gamename.clear();

				if (setupGame(info))
					startupGame();
				else
					startupPentagramMenu();
			}
			else {
				perr << "Game '" << change_gamename << "' not found" << std::endl;
				change_gamename.clear();
			}
		}

		if (!error_message.empty())
		{
			MessageBoxGump::Show(error_title, error_message, 0xFF8F3030);
			error_title.clear();
			error_message.clear();
		}

		// Do a delay
		SDL_Delay(5);
	}
}


// conAutoPaint hackery
void GUIApp::conAutoPaint(void)
{
	GUIApp *app = GUIApp::get_instance();
	if (app && !app->isPainting()) app->paint();
}

#if defined(WIN32) && defined(I_AM_COLOURLESS_EXPERIMENTING_WITH_HW_CURSORS)

WNDPROC oldWindowProc = 0;
bool allow_set_cursor = true;

LRESULT CALLBACK GUIApp::myWindowProc(
  HWND hwnd,      // handle to window
  UINT uMsg,      // message identifier
  WPARAM wParam,  // first message parameter
  LPARAM lParam   // second message parameter
  )
{
	if (uMsg == WM_SETCURSOR) {
		if ( allow_set_cursor = (LOWORD(lParam) == HTCLIENT) ) {
			GUIApp *app = GUIApp::get_instance();
			GameData *gamedata = app->gamedata;

			if (gamedata) {
				Shape* mouse = gamedata->getMouse();
				if (mouse) {
					int frame = app->getMouseFrame();
					if (frame >= 0)
						SetCursor(app->hwcursors[frame].hCursor);
				}
			}
			return TRUE;
		}
	}

	return CallWindowProc(oldWindowProc, hwnd, uMsg, wParam, lParam);
}

void GUIApp::CreateHWCursors()
{
	Shape* mouse = gamedata->getMouse();
	hwcursors = new HWMouseCursor[mouse->frameCount()];
	std::memset (hwcursors, 0, sizeof(HWMouseCursor) * mouse->frameCount());

	for (uint32 frame = 0; frame < mouse->frameCount(); frame++)
	{
		ShapeFrame *f = mouse->getFrame(frame);
		uint32 bpp = BaseSoftRenderSurface::format.s_bpp;
		int buf_width = f->width;
		int buf_height = f->height;

		// DIB must be dword aligned
		if (bpp != 32) buf_width = (buf_width+1)&~1;	

		uint8 *buf = new uint8 [bpp/8 * buf_width * buf_height];

		RenderSurface *surf;

		if (bpp == 32)
			surf = new SoftRenderSurface<uint32>(buf_width, buf_height, buf);
		else
			surf = new SoftRenderSurface<uint16>(buf_width, buf_height, buf);

		surf->BeginPainting();
		surf->Fill32(0x00FF00FF, 0, 0, buf_width, buf_height);
		surf->Paint(mouse, frame, f->xoff, f->yoff);
		surf->EndPainting();

		int clear_col = PACK_RGB8(0xFF,0x00,0xFF);

		//
		// Mask
		//

		// 1 bit bitmap must be word aligned 
		uint32 bit_width = (buf_width+15)&~15;
		uint8 *buf_mask = new uint8 [bit_width/8 * buf_height*2];

		// Clear it
		std::memset(buf_mask, 0x00, bit_width/8 * buf_height * 2);


		if (bpp == 32)
		{
			uint32 *buf32 = (uint32*)buf;
			for (int y = 0; y < buf_height; y++)
			{
				for (int x = 0; x < buf_width; x++)
				{
					bool black = (x & 1) == (y & 1);
					uint32 bit = y * bit_width + x;
					uint32 byte = bit/8;
					bit = 7-(bit % 8);

					// If background is clear colour, mask it out
					if (buf32[buf_width*y + x] == clear_col)
					{
						buf32[buf_width*y + x] = 0;
						buf_mask[byte] |=   1<< bit;
					}
					// Make any non black make white
					else if (buf32[buf_width*y + x])
						buf_mask[byte+((buf_height*bit_width)/8)] |= 1<<bit;
				}
			}
		}
		else
		{
			uint16 *buf16 = (uint16*)buf;
			for (int y = 0; y < buf_height; y++)
			{
				for (int x = 0; x < buf_width; x++)
				{
					bool black = (x & 1) == (y & 1);
					uint32 bit = y * bit_width + x;
					uint32 byte = bit/8;
					bit = 7-(bit % 8);

					// If background is clear colour, mask it out
					if (buf16[buf_width*y + x] == clear_col)
					{
						buf16[buf_width*y + x] = 0;
						buf_mask[byte] |=   1<< bit;
					}
					// Make any non black make white
					else if (buf16[buf_width*y + x])
						buf_mask[byte+((buf_height*bit_width)/8)] |= 1<<bit;
				}
			}
		}

		// Create an icon for our cursor
		ICONINFO iconinfo;
		iconinfo.fIcon = FALSE;
		iconinfo.xHotspot = f->xoff;
		iconinfo.yHotspot = f->yoff;
		iconinfo.hbmMask = CreateBitmap(buf_width, buf_height, 1, 1, buf_mask);
		iconinfo.hbmColor = CreateBitmap(buf_width, buf_height, 1, bpp, buf);

		hwcursors[frame].hCursor = CreateIconIndirect (&iconinfo);

		DeleteObject(iconinfo.hbmMask);
		DeleteObject(iconinfo.hbmColor);

		delete [] buf;
		delete [] buf_mask;
		delete surf;
	}

	// Lets screw with the window class
	SDL_SysWMinfo info;
	info.version.major = SDL_MAJOR_VERSION;
	info.version.minor = SDL_MINOR_VERSION;
	info.version.patch = SDL_PATCHLEVEL;
	SDL_GetWMInfo(&info);
	oldWindowProc = (WNDPROC) GetWindowLongPtr(info.window, GWLP_WNDPROC);
	SetWindowLongPtr(info.window, GWLP_WNDPROC, (LONG) myWindowProc);
	
}
#endif

// Paint the screen
void GUIApp::paint()
{
    static long prev = 0;
    static long t = 0;
    static long tdiff = 0;
    static long tpaint = 0;
    long now = SDL_GetTicks();

	if(!screen) // need to worry if the graphics system has been started. Need nicer way.
		return;

    if (prev != 0)
        tdiff += now - prev;
    prev = now;
    ++t;
    
	painting = true;

	// Begin painting
	screen->BeginPainting();

	// We need to get the dims
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);

    tpaint -= SDL_GetTicks();
	desktopGump->Paint(screen, lerpFactor, false);
	tpaint += SDL_GetTicks();

	// Mouse
	if (gamedata) {
		Shape* mouse = gamedata->getMouse();
		if (mouse) {
			int frame = getMouseFrame();
			if (frame >= 0)
			{
#if defined(WIN32) && defined(I_AM_COLOURLESS_EXPERIMENTING_WITH_HW_CURSORS)
				if (allow_set_cursor) SetCursor(hwcursors[frame].hCursor);
#else
				screen->Paint(mouse, frame, mouseX, mouseY, true);
#endif
			}
			else if (frame == -2)
				screen->Blit(defMouse, 0, 0, defMouse->width, defMouse->height, mouseX, mouseY);
		}
	}
	else {
		if (getMouseFrame() != -1) 
			screen->Blit(defMouse, 0, 0, defMouse->width, defMouse->height, mouseX, mouseY);
	}
    
	if (drawRenderStats)
	{
		static long diff = 0;
		static long fps = 0;
		static long paint = 0;
		char buf[256] = {'\0'};
		FixedWidthFont *confont = con.GetConFont();
		int v_offset = 0;
		int char_w = confont->width;

		if (tdiff >= 250) {
			diff = tdiff / t;
			paint = tpaint / t;
			fps = 1000 * t / tdiff;
			t = 0;
			tdiff = 0;
			tpaint = 0;
		}

		snprintf(buf, 255, "Rendering time %li ms %li FPS ", diff, fps);
		screen->PrintTextFixed(confont, buf, dims.w-char_w*strlen(buf), v_offset);
		v_offset += confont->height;

		snprintf(buf, 255, "Paint Gumps %li ms ", paint);
		screen->PrintTextFixed(confont, buf, dims.w-char_w*strlen(buf), v_offset);
		v_offset += confont->height;

		snprintf(buf, 255, "t %02d:%02d gh %i ", I_getTimeInMinutes(0,0), I_getTimeInSeconds(0,0)%60, I_getTimeInGameHours(0,0));
		screen->PrintTextFixed(confont, buf, dims.w-char_w*strlen(buf), v_offset);
		v_offset += confont->height;
	}

	// End painting
	screen->EndPainting();

	painting = false;
}

bool GUIApp::isMouseDown(MouseButton button)
{
	return (mouseButton[button].state & MBS_DOWN);
}

int GUIApp::getMouseLength(int mx, int my)
{
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);
	// For now, reference point is (near) the center of the screen
	int dx = mx - dims.w/2;
	int dy = (dims.h/2+14) - my; //! constant

	int shortsq = (dims.w / 8);
	if (dims.h / 6 < shortsq)
		shortsq = (dims.h / 6);
	shortsq = shortsq*shortsq;
	
	int mediumsq = ((dims.w * 4) / 10);
	if (((dims.h * 4) / 10) < mediumsq)
		mediumsq = ((dims.h * 4) / 10);
	mediumsq = mediumsq * mediumsq;
	
	int dsq = dx*dx+dy*dy;
	
	// determine length of arrow
	if (dsq <= shortsq) {
		return 0;
	} else if (dsq <= mediumsq) {
		return 1;
	} else {
		return 2;
	}
}

int GUIApp::getMouseDirection(int mx, int my)
{
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);
	// For now, reference point is (near) the center of the screen
	int dx = mx - dims.w/2;
	int dy = (dims.h/2+14) - my; //! constant

	return ((Get_direction(dy*2, dx))+1)%8;
}

int GUIApp::getMouseFrame()
{
	// Ultima 8 mouse cursors:

	// 0-7 = short (0 = up, 1 = up-right, 2 = right, ...)
	// 8-15 = medium
	// 16-23 = long
	// 24 = blue dot
	// 25-32 = combat
	// 33 = red dot
	// 34 = target
	// 35 = pentagram
	// 36 = skeletal hand
	// 38 = quill
	// 39 = magnifying glass
	// 40 = red cross

	MouseCursor cursor = cursors.top();

	if (flashingcursor > 0) {
		if (SDL_GetTicks() < flashingcursor + 250)
			cursor = MOUSE_CROSS;
		else
			flashingcursor = 0;
	}


	switch (cursor) {
	case MOUSE_NORMAL:
	{
		bool combat = false;
		MainActor* av = getMainActor();
		if (av) { combat = av->isInCombat(); }

		// Calculate frame based on direction
		int frame = getMouseDirection(mouseX, mouseY);

		/** length --- frame offset
		 *    0              0
		 *    1              8
		 *    2             16
		 *  combat          25
		 **/
		int offset = getMouseLength(mouseX, mouseY) * 8;
		if (combat && offset != 16) //combat mouse is off if running
			offset = 25;
		return frame + offset;
	}
	//!! constants...
	case MOUSE_NONE: return -1;
	case MOUSE_POINTER: return -2;
	case MOUSE_TARGET: return 34;
	case MOUSE_PENTAGRAM: return 35;
	case MOUSE_HAND: return 36;
	case MOUSE_QUILL: return 38;
	case MOUSE_MAGGLASS: return 39;
	case MOUSE_CROSS: return 40;
	default: return -1;
	}

}

void GUIApp::setMouseCoords(int mx, int my)
{
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);

	if (mx < dims.x)
		mx = dims.x;
	else if (mx > dims.w)
		mx = dims.w;

	if (my < dims.y)
		my = dims.y;
	else if (my > dims.h)
		my = dims.h;

	mouseX = mx; mouseY = my;
	Gump * gump = desktopGump->OnMouseMotion(mx, my);
	if (gump && mouseOverGump != gump->getObjId())
	{
		Gump * oldGump = getGump(mouseOverGump);
		std::list<Gump*> oldgumplist;
		std::list<Gump*> newgumplist;

		// create lists of parents of old and new 'mouseover' gumps
		if (oldGump) {
			while (oldGump) {
				oldgumplist.push_front(oldGump);
				oldGump = oldGump->GetParent();
			}
		}
		Gump* newGump = gump;
		while (newGump) {
			newgumplist.push_front(newGump);
			newGump = newGump->GetParent();
		}

		std::list<Gump*>::iterator olditer = oldgumplist.begin();
		std::list<Gump*>::iterator newiter = newgumplist.begin();

		// strip common prefix from lists
		while (olditer != oldgumplist.end() &&
			   newiter != newgumplist.end() &&
			   *olditer == *newiter)
		{
			++olditer; ++newiter;
		}

		// send events to remaining gumps
		for (; olditer != oldgumplist.end(); ++olditer)
			(*olditer)->OnMouseLeft();

		mouseOverGump = gump->getObjId();

		for (; newiter != newgumplist.end(); ++newiter)
			(*newiter)->OnMouseOver();
	}

	if (dragging == DRAG_NOT) {
		if (mouseButton[BUTTON_LEFT].state & MBS_DOWN) {
			int startx = mouseButton[BUTTON_LEFT].downX;
			int starty = mouseButton[BUTTON_LEFT].downY;
			if (abs(startx - mx) > 2 ||
				abs(starty - my) > 2)
			{
				startDragging(startx, starty);
			}
		}
	}

	if (dragging == DRAG_OK || dragging == DRAG_TEMPFAIL) {
		moveDragging(mx, my);
	}
}

void GUIApp::setMouseCursor(MouseCursor cursor)
{
	cursors.pop();
	cursors.push(cursor);
}

void GUIApp::flashCrossCursor()
{
	flashingcursor = SDL_GetTicks();
}

void GUIApp::pushMouseCursor()
{
	cursors.push(MOUSE_NORMAL);
}

void GUIApp::popMouseCursor()
{
	cursors.pop();
}

void GUIApp::GraphicSysInit()
{
	settingman->setDefault("fullscreen", false);
	settingman->setDefault("width", 640);
	settingman->setDefault("height", 480);
	settingman->setDefault("bpp", 32);

	bool new_fullscreen;
	int width, height, bpp;
	settingman->get("fullscreen", new_fullscreen);
	settingman->get("width", width);
	settingman->get("height", height);
	settingman->get("bpp", bpp);

#ifdef UNDER_CE
	width = 240;
	height = 320;
#endif

#if 0
	// store values in user's config file
	settingman->set("width", width);
	settingman->set("height", height);
	settingman->set("bpp", bpp);
	settingman->set("fullscreen", new_fullscreen);
#endif

	if (screen) {
		Pentagram::Rect old_dims;
		screen->GetSurfaceDims(old_dims);
		if (new_fullscreen == fullscreen && width == old_dims.w && height == old_dims.h) return;
		bpp = RenderSurface::format.s_bpp;

		delete screen;
	}
	screen = 0;

	fullscreen = new_fullscreen;

	// Set Screen Resolution
	con.Printf(MM_INFO, "Setting Video Mode %ix%ix%i %s...\n", width, height, bpp, fullscreen?"fullscreen":"windowed");

	RenderSurface *new_screen = RenderSurface::SetVideoMode(width, height, bpp, fullscreen, false);

	if (!new_screen)
	{
		perr << "Unable to set new video mode. Trying 640x480x32 windowed" << std::endl;
		new_screen = RenderSurface::SetVideoMode(640, 480, 32, fullscreen=false, false);
	}

	if (!new_screen)
	{
		perr << "Unable to set video mode. Exiting." << std::endl;
		std::exit(-1);
	}

	if (desktopGump) {
		palettemanager->RenderSurfaceChanged(new_screen);
		static_cast<DesktopGump*>(desktopGump)->RenderSurfaceChanged(new_screen);
		screen = new_screen;
		paint();
		return;
	}

	// set window title
	SDL_WM_SetCaption("Pentagram", "");

	// setup normal mouse cursor
	con.Print(MM_INFO, "Loading Default Mouse Cursor...\n");
	IDataSource *dm = filesystem->ReadFile("@data/mouse.tga");
	if (dm) defMouse = Texture::Create(dm, "@data/mouse.tga");
	else defMouse = 0;
	if (!defMouse)
	{
		perr << "Unable to load '@data/mouse.tga'" << ". Exiting" << std::endl;
		std::exit(-1);
	}
	delete dm;
	pushMouseCursor();

	std::string alt_confont;
	bool confont_loaded = false;

	if (settingman->get("console_font", alt_confont)) {
		con.Print(MM_INFO, "Alternate console font found...\n");
		confont_loaded = LoadConsoleFont(alt_confont);
    }

	if (!confont_loaded) {
		con.Print(MM_INFO, "Loading default console font...\n");
		if (!LoadConsoleFont("@data/fixedfont.ini")) {
			perr << "Failed to load console font. Exiting" << std::endl;
			std::exit(-1);
		}
	}

	desktopGump = new DesktopGump(0,0, width, height);
	desktopGump->InitGump(0);
	desktopGump->MakeFocus();

	scalerGump = new ScalerGump(0, 0, width, height);
	scalerGump->InitGump(0);

	consoleGump = new ConsoleGump(0, 0, width, height);
	consoleGump->InitGump(0);

	Pentagram::Rect scaled_dims;
	scalerGump->GetDims(scaled_dims);

	inverterGump = new InverterGump(0, 0, scaled_dims.w, scaled_dims.h);
	inverterGump->InitGump(0);

	screen = new_screen;

	bool ttf_antialiasing = true;
	settingman->setDefault("ttf_antialiasing", true);
	settingman->get("ttf_antialiasing", ttf_antialiasing);

	fontmanager = new FontManager(ttf_antialiasing);
	palettemanager = new PaletteManager(new_screen);

	// TODO: assign names to these fontnumbers somehow
	fontmanager->loadTTFont(0, "Vera.ttf", 18, 0xFFFFFF, 0);
	fontmanager->loadTTFont(1, "VeraBd.ttf", 16, 0xFFFFFF, 0);
	// GameWidget's version number information:
	fontmanager->loadTTFont(2, "Vera.ttf", 8, 0xA0A0A0, 0);

	bool faded_modal = true;
	settingman->setDefault("fadedModal", faded_modal);
	settingman->get("fadedModal", faded_modal);
	DesktopGump::SetFadedModal(faded_modal);

	paint();
}

void GUIApp::changeVideoMode(int width, int height, int new_fullscreen)
{
	if (new_fullscreen == -2) settingman->set("fullscreen", !fullscreen);
	else if (new_fullscreen == 0) settingman->set("fullscreen", false);
	else if (new_fullscreen == 1) settingman->set("fullscreen", true);

	if (width > 0) settingman->set("width", width);
	if (height > 0) settingman->set("height", height);

	GraphicSysInit();
}

bool GUIApp::LoadConsoleFont(std::string confontini)
{
	// try to load the file
	con.Printf(MM_INFO, "Loading console font config: %s... ", confontini.c_str());
	if(configfileman->readConfigFile(confontini, "confont", true))
		pout << "Ok" << std::endl;
	else {
		pout << "Failed" << std::endl;
		return false;
	}

	FixedWidthFont *confont = FixedWidthFont::Create("confont");

	if (!confont) {
		perr << "Failed to load Console Font." << std::endl;
		return false;
	}

	con.SetConFont(confont);

	return true;
}

void GUIApp::enterTextMode(Gump *gump)
{
	uint16 key;
	for (key=0; key < HID_LAST; ++key)
	{
		if (down[key])
		{
			down[key] = 0;
			lastDown[key] = 0;
			hidmanager->handleEvent((HID_Key) key, HID_EVENT_RELEASE);
		}
	}
		
	if (!textmodes.empty()) {
		textmodes.remove(gump->getObjId());
	} else {
		SDL_EnableUNICODE(1);
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
							SDL_DEFAULT_REPEAT_INTERVAL);
	}
	textmodes.push_front(gump->getObjId());
}

void GUIApp::leaveTextMode(Gump *gump)
{
	if (textmodes.empty()) return;
	textmodes.remove(gump->getObjId());
	if (textmodes.empty()) {
		SDL_EnableUNICODE(0);
		SDL_EnableKeyRepeat(0, 0);
	}
}

void GUIApp::handleEvent(const SDL_Event& event)
{
	uint32 now = SDL_GetTicks();
	HID_Key key = HID_LAST;
	HID_Event evn = HID_EVENT_LAST;
	SDL_Event fakeevent;
	int mx, my;
	bool handled = false;
	GUIApp * app = GUIApp::get_instance();
	app->getMouseCoords(mx, my);
		
	switch (event.type) {
		case SDL_KEYDOWN:
			key = HID_translateSDLKey(event.key.keysym.sym);
			evn = HID_EVENT_DEPRESS;
		break;
		case SDL_KEYUP:
			key = HID_translateSDLKey(event.key.keysym.sym);
			evn = HID_EVENT_RELEASE;
			if (dragging == DRAG_NOT)
			{
				switch (event.key.keysym.sym) {
					case SDLK_q: // Quick quit
#ifndef MACOSX
						if (event.key.keysym.mod & KMOD_CTRL)
							ForceQuit();
#else
						if (event.key.keysym.mod & KMOD_META)
							ForceQuit();
#endif
					return;
					default: break;
				}
			}
		break;
		case SDL_MOUSEBUTTONDOWN:
			key = HID_translateSDLMouseButton(event.button.button);
			evn = HID_EVENT_DEPRESS;
		break;
		case SDL_MOUSEBUTTONUP:
			key = HID_translateSDLMouseButton(event.button.button);
			evn = HID_EVENT_RELEASE;
		break;
		case SDL_JOYBUTTONDOWN:
			key = HID_translateSDLJoystickButton(event.jbutton.button);
			evn = HID_EVENT_DEPRESS;
			switch (event.jbutton.button) {
				case 10:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_F8;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 9:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_F7;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 8:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_m;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 7:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_z;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 6:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_SPACE;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 5:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_RETURN;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 4:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_ESCAPE;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 3:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_c;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 2:
					fakeevent.type = SDL_KEYDOWN;
					fakeevent.key.type = SDL_KEYDOWN;
					fakeevent.key.keysym.sym = SDLK_i;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 1:			
					fakeevent.type = SDL_MOUSEBUTTONDOWN;
					fakeevent.button.button = SDL_BUTTON_LEFT;
					fakeevent.button.x = mx;
					fakeevent.button.y = my;
					SDL_PushEvent (&fakeevent);
					break;
				case 0:			
					fakeevent.type = SDL_MOUSEBUTTONDOWN;
					fakeevent.button.button = SDL_BUTTON_RIGHT;
					fakeevent.button.x = mx;
					fakeevent.button.y = my;
					SDL_PushEvent (&fakeevent);
					break;
				default:
					break;
			}
		break;
		case SDL_JOYBUTTONUP:
			key = HID_translateSDLJoystickButton(event.jbutton.button);
			evn = HID_EVENT_RELEASE;
			switch (event.jbutton.button) {
				case 10:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_F8;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 9:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_F7;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 8:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_m;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 7:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_z;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 6:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_SPACE;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 5:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_RETURN;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 4:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_ESCAPE;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 3:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_c;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 2:
					fakeevent.type = SDL_KEYUP;
					fakeevent.key.type = SDL_KEYUP;
					fakeevent.key.keysym.sym = SDLK_i;
					fakeevent.key.keysym.mod = KMOD_NONE;
					SDL_PushEvent (&fakeevent);
					break;
				case 1:			
					fakeevent.type = SDL_MOUSEBUTTONUP;
					fakeevent.button.button = SDL_BUTTON_LEFT;
					fakeevent.button.x = mx;
					fakeevent.button.y = my;
					SDL_PushEvent (&fakeevent);
					break;
				case 0:			
					fakeevent.type = SDL_MOUSEBUTTONUP;
					fakeevent.button.button = SDL_BUTTON_RIGHT;
					fakeevent.button.x = mx;
					fakeevent.button.y = my;
					SDL_PushEvent (&fakeevent);
					break;
				default:
					break;
			}
		break;
		case SDL_MOUSEMOTION:
		{
			int mx = event.button.x;
			int my = event.button.y;
			setMouseCoords(mx, my);
		}
		break;

		case SDL_QUIT:
			isRunning = false;
		break;
		case SDL_ACTIVEEVENT:
			// pause when lost focus?
		break;
	}

	if (dragging == DRAG_NOT && evn == HID_EVENT_DEPRESS) {
		if (hidmanager->handleEvent(key, HID_EVENT_PREEMPT))
			return;
	}

	// Text mode input. A few hacks here
	if (!textmodes.empty()) {
		Gump *gump = 0;

		while (!textmodes.empty())
		{
			gump = p_dynamic_cast<Gump*>(objectmanager->getObject(textmodes.front()));
			if (gump) break;

			textmodes.pop_front();
		}

		if (gump) {
			switch (event.type) {
				case SDL_KEYDOWN:
#ifdef WIN32 
					// Paste from Clip-Board on Ctrl-V - Note this should be a flag of some sort
					if (event.key.keysym.sym == SDLK_v && event.key.keysym.mod & KMOD_CTRL)
					{
						if (!IsClipboardFormatAvailable(CF_TEXT)) 
							return ; 
						if (!OpenClipboard(NULL)) 
							return; 

						HGLOBAL hglb = GetClipboardData(CF_TEXT); 
						if (hglb != NULL) 
						{ 
							LPTSTR lptstr = reinterpret_cast<LPTSTR>(GlobalLock(hglb)); 
							if (lptstr != NULL) 
							{ 
								// Only read the first line of text
								while (*lptstr >= ' ') gump->OnTextInput(*lptstr++);

								GlobalUnlock(hglb); 
							} 
						} 
						CloseClipboard(); 
						return;
					}
#endif

#ifdef MACOSX
					// Paste from Clip-Board on Command-V - Note this should be a flag of some sort
					if (event.key.keysym.sym == SDLK_v && event.key.keysym.mod & KMOD_META)
					{
						const char * str = macosxPasteboardText();
						if (str != NULL) 
						{ 
							// Only read the first line of text
							while (*str >= ' ')
								gump->OnTextInput(*str++);
						} 
						return;
					}
#endif

					if (event.key.keysym.unicode >= ' ' &&
						event.key.keysym.unicode <= 255 &&
						!(event.key.keysym.unicode >= 0x7F && // control chars
						  event.key.keysym.unicode <= 0x9F))
					{
						gump->OnTextInput(event.key.keysym.unicode);
					}

					gump->OnKeyDown(event.key.keysym.sym, event.key.keysym.mod);
					return;

				case SDL_KEYUP:
					gump->OnKeyUp(event.key.keysym.sym);
					return;

				default: break;
			}
		}
	}

	// Old style input begins here
	switch (event.type) {

	//!! TODO: handle mouse handedness. (swap left/right mouse buttons here)

	// most of these events will probably be passed to a gump manager,
	// since almost all (all?) user input will be handled by a gump

	
	case SDL_MOUSEBUTTONDOWN:
	{
		int button = event.button.button;
		int mx = event.button.x;
		int my = event.button.y;

		if (button >= MOUSE_LAST)
			break;

		Gump *mousedowngump = desktopGump->OnMouseDown(button, mx, my);
		if (mousedowngump)
		{
			mouseButton[button].downGump = mousedowngump->getObjId();
			handled = true;
		}
		else
			mouseButton[button].downGump = 0;

		mouseButton[button].curDown = now;
		mouseButton[button].downX = mx;
		mouseButton[button].downY = my;
		mouseButton[button].state |= MBS_DOWN;
		mouseButton[button].state &= ~MBS_HANDLED;

		if (now - mouseButton[button].lastDown < DOUBLE_CLICK_TIMEOUT) {
			if (dragging == DRAG_NOT) {
				Gump* gump = getGump(mouseButton[button].downGump);
				if (gump)
				{
					int mx2 = mx, my2 = my;
					Gump *parent = gump->GetParent();
					if (parent) parent->ScreenSpaceToGump(mx2,my2);
					gump->OnMouseDouble(button, mx2, my2);
				}
				mouseButton[button].state |= MBS_HANDLED;
				mouseButton[button].lastDown = 0;
			}
		}
		mouseButton[button].lastDown = now;
	}
	break;

	case SDL_MOUSEBUTTONUP:
	{
		int button = event.button.button;
		int mx = event.button.x;
		int my = event.button.y;

		if (button >= MOUSE_LAST)
			break;

		mouseButton[button].state &= ~MBS_DOWN;

		// Need to store the last down position of the mouse
		// when the button is released.
		mouseButton[button].downX = mx;
		mouseButton[button].downY = my;

		// Always send mouse up to the gump
		Gump* gump = getGump(mouseButton[button].downGump);
		if (gump)
		{
			int mx2 = mx, my2 = my;
			Gump *parent = gump->GetParent();
			if (parent)
				parent->ScreenSpaceToGump(mx2,my2);
			gump->OnMouseUp(button, mx2, my2);
			handled = true;
		}

		if (button == BUTTON_LEFT && dragging != DRAG_NOT) {
			stopDragging(mx, my);
			handled = true;
			break;
		}
	}
	break;

	case SDL_KEYDOWN:
	{
		if (dragging != DRAG_NOT) break;

		/*
		switch (event.key.keysym.sym) {
			case SDLK_KP_PLUS: {
				midi_volume+=8;
				if (midi_volume>255) midi_volume =255;
				pout << "Midi Volume is now: " << midi_volume << std::endl; 
				if (midi_driver) midi_driver->setGlobalVolume(midi_volume);
			} break;
			case SDLK_KP_MINUS: {
				midi_volume-=8;
				if (midi_volume<0) midi_volume = 0;
				pout << "Midi Volume is now: " << midi_volume << std::endl; 
				if (midi_driver) midi_driver->setGlobalVolume(midi_volume);
			} break;
			default:
				break;
		}
		*/
	}
	break;

	// any more useful events?

	default:
		break;
	}

	if (dragging == DRAG_NOT && ! handled) {
		if (hidmanager->handleEvent(key, evn))
			handled = true;
		if (evn == HID_EVENT_DEPRESS)
		{
			down[key] = 1;
			if (now - lastDown[key] < DOUBLE_CLICK_TIMEOUT &&
				lastDown[key] != 0)
			{
				if (hidmanager->handleEvent(key, HID_EVENT_DOUBLE))
					handled = true;
				lastDown[key] = 0;
			}
			else
			{
				lastDown[key] = now;
			}
		}
		else if (evn == HID_EVENT_RELEASE)
		{
			down[key] = 0;
			if (now - lastDown[key] > DOUBLE_CLICK_TIMEOUT &&
				lastDown[key] != 0)
			{
				lastDown[key] = 0;
			}
		}
	}
}

void GUIApp::handleDelayedEvents()
{
	uint32 now = SDL_GetTicks();
	uint16 key;
	int button;
	for (button = 0; button < MOUSE_LAST; ++button) {
		if (!(mouseButton[button].state & (MBS_HANDLED | MBS_DOWN)) &&
			now - mouseButton[button].lastDown > DOUBLE_CLICK_TIMEOUT)
		{
			Gump* gump = getGump(mouseButton[button].downGump);
			if (gump)
			{
				int mx = mouseButton[button].downX;
				int my = mouseButton[button].downY;
				Gump *parent = gump->GetParent();
				if (parent) parent->ScreenSpaceToGump(mx,my);
				gump->OnMouseClick(button, mx, my);
			}

			mouseButton[button].downGump = 0;
			mouseButton[button].state |= MBS_HANDLED;
		}
	}

	for (key=0; key < HID_LAST; ++key)
	{
		if (now - lastDown[key] > DOUBLE_CLICK_TIMEOUT &&
			lastDown[key] != 0 && down[key] == 0)
		{
			lastDown[key] = 0;
			hidmanager->handleEvent((HID_Key) key, HID_EVENT_CLICK);
		}
	}

}

void GUIApp::startDragging(int startx, int starty)
{
	setDraggingOffset(0,0); // initialize

	dragging_objid = desktopGump->TraceObjId(startx, starty);
	
	Gump *gump = getGump(dragging_objid);
	Item *item = getItem(dragging_objid);
	
	// for a Gump, notify the Gump's parent that we started
	// dragging:
	if (gump) {
		Gump *parent = gump->GetParent();
		assert(parent); // can't drag root gump
		int px = startx, py = starty;
		parent->ScreenSpaceToGump(px, py);
		if (gump->IsDraggable() && parent->StartDraggingChild(gump, px, py))
			dragging = DRAG_OK;
		else {
			dragging_objid = 0;
			return;
		}
	} else
	// for an Item, notify the gump the item is in that we started dragging
	if (item) {
		// find gump item was in
		gump = desktopGump->FindGump(startx, starty);
		int gx = startx, gy = starty;
		gump->ScreenSpaceToGump(gx, gy);
		bool ok = !isAvatarInStasis() &&
			gump->StartDraggingItem(item,gx,gy);
		if (!ok) {
			dragging = DRAG_INVALID;
		} else {
			dragging = DRAG_OK;
			
			// this is the gump that'll get StopDraggingItem
			dragging_item_startgump = gump->getObjId();
			
			// this is the gump the item is currently over
			dragging_item_lastgump = gump->getObjId();
		}
	} else {
		dragging = DRAG_INVALID;
	}

#if 0
	Object* obj = ObjectManager::get_instance()->getObject(dragging_objid);
	perr << "Dragging object " << dragging_objid << " (class=" << (obj ? obj->GetClassType().class_name : "NULL") << ")" << std::endl;
#endif

	pushMouseCursor();
	setMouseCursor(MOUSE_NORMAL);
	
	// pause the kernel
	kernel->pause();
	
	mouseButton[BUTTON_LEFT].state |= MBS_HANDLED;

	if (dragging == DRAG_INVALID) {
		setMouseCursor(MOUSE_CROSS);
	}
}

void GUIApp::moveDragging(int mx, int my)
{
	Gump* gump = getGump(dragging_objid);
	Item *item = getItem(dragging_objid);

	setMouseCursor(MOUSE_NORMAL);
	
	// for a gump, notify Gump's parent that it was dragged
	if (gump) {
		Gump *parent = gump->GetParent();
		assert(parent); // can't drag root gump
		int px = mx, py = my;
		parent->ScreenSpaceToGump(px, py);
		parent->DraggingChild(gump, px, py);
	} else
	// for an item, notify the gump it's on
	if (item) {
		gump = desktopGump->FindGump(mx, my);
		assert(gump);
			
		if (gump->getObjId() != dragging_item_lastgump) {
			// item switched gump, so notify previous gump item left
			Gump *last = getGump(dragging_item_lastgump);
			if (last) last->DraggingItemLeftGump(item);
		}
		dragging_item_lastgump = gump->getObjId();
		int gx = mx, gy = my;
		gump->ScreenSpaceToGump(gx, gy);
		bool ok = gump->DraggingItem(item,gx,gy);
		if (!ok) {
			dragging = DRAG_TEMPFAIL;
		} else {
			dragging = DRAG_OK;
		}
	} else {
		CANT_HAPPEN();
	}

	if (dragging == DRAG_TEMPFAIL) {
		setMouseCursor(MOUSE_CROSS);
	}
}


void GUIApp::stopDragging(int mx, int my)
{
//	perr << "Dropping object " << dragging_objid << std::endl;
	
	Gump *gump = getGump(dragging_objid);
	Item *item = getItem(dragging_objid);
	// for a Gump: notify parent
	if (gump) {
		Gump *parent = gump->GetParent();
		assert(parent); // can't drag root gump
		parent->StopDraggingChild(gump);
	} else 
	// for an item: notify gumps
	if (item) {
		if (dragging != DRAG_INVALID) {
			Gump *startgump = getGump(dragging_item_startgump);
			assert(startgump); // can't have disappeared
			bool moved = (dragging == DRAG_OK);

			if (dragging != DRAG_OK) {
				Gump *last = getGump(dragging_item_lastgump);
				if (last && last != startgump)
					last->DraggingItemLeftGump(item);
			}

			startgump->StopDraggingItem(item, moved);
		}
		
		if (dragging == DRAG_OK) {
			item->movedByPlayer();

			gump = desktopGump->FindGump(mx, my);
			int gx = mx, gy = my;
			gump->ScreenSpaceToGump(gx, gy);
			gump->DropItem(item,gx,gy);
		}
	} else {
		assert(dragging == DRAG_INVALID);
	}

	dragging = DRAG_NOT;

	kernel->unpause();

	popMouseCursor();
}

void GUIApp::writeSaveInfo(ODataSource* ods)
{
	time_t t = std::time(0);
	struct tm *timeinfo = localtime (&t);
	ods->write2(static_cast<uint16>(timeinfo->tm_year + 1900));
	ods->write1(static_cast<uint8>(timeinfo->tm_mon+1));
	ods->write1(static_cast<uint8>(timeinfo->tm_mday));
	ods->write1(static_cast<uint8>(timeinfo->tm_hour));
	ods->write1(static_cast<uint8>(timeinfo->tm_min));
	ods->write1(static_cast<uint8>(timeinfo->tm_sec));
	ods->write4(save_count);
	ods->write4(getGameTimeInSeconds());

	uint8 c = (has_cheated ? 1 : 0);
	ods->write1(c);

	// write game-specific info
	game->writeSaveInfo(ods);
}

bool GUIApp::saveGame(std::string filename, std::string desc,
					  bool ignore_modals)
{
	// Don't allow saving with Modals open
	if (!ignore_modals && desktopGump->FindGump<ModalGump>()) {
		pout << "Can't save: modal gump open." << std::endl;
		return false;
	}

	// Don't allow saving when avatar is dead.
	// (Avatar is flagged dead by usecode when you finish the game as well.)
	MainActor* av = getMainActor();
	if (!av || (av->getActorFlags() & Actor::ACT_DEAD)) {
		pout << "Can't save: game over." << std::endl;
		return false;
	}

	pout << "Saving..." << std::endl;

	pout << "Savegame file: " << filename << std::endl;
	pout << "Description: " << desc << std::endl;

	// Hack - don't save mouse over status for gumps
	Gump * gump = getGump(mouseOverGump);
	if (gump) gump->OnMouseLeft();

	ODataSource* ods = filesystem->WriteFile(filename);
	if (!ods) return false;

	save_count++;

	SavegameWriter* sgw = new SavegameWriter(ods);
	sgw->writeVersion(Pentagram::savegame_version);
	sgw->writeDescription(desc);

	// We'll make it 2KB initially
	OAutoBufferDataSource buf(2048);

	gameinfo->save(&buf);
	sgw->writeFile("GAME", &buf);
	buf.clear();

	writeSaveInfo(&buf);
	sgw->writeFile("INFO", &buf);
	buf.clear();

	kernel->save(&buf);
	sgw->writeFile("KERNEL", &buf);
	buf.clear();

	objectmanager->save(&buf);
	sgw->writeFile("OBJECTS", &buf);
	buf.clear();

	world->save(&buf);
	sgw->writeFile("WORLD", &buf);
	buf.clear();

	world->saveMaps(&buf);
	sgw->writeFile("MAPS", &buf);
	buf.clear();

	world->getCurrentMap()->save(&buf);
	sgw->writeFile("CURRENTMAP", &buf);
	buf.clear();

	ucmachine->saveStrings(&buf);
	sgw->writeFile("UCSTRINGS", &buf);
	buf.clear();

	ucmachine->saveGlobals(&buf);
	sgw->writeFile("UCGLOBALS", &buf);
	buf.clear();

	ucmachine->saveLists(&buf);
	sgw->writeFile("UCLISTS", &buf);
	buf.clear();

	save(&buf);
	sgw->writeFile("APP", &buf);
	buf.clear();

	sgw->finish();

	delete sgw;

	// Restore mouse over
	if (gump) gump->OnMouseOver();

	settingman->set("lastSave", filename);

	pout << "Done" << std::endl;

	return true;
}

void GUIApp::resetEngine()
{
	con.Print(MM_INFO, "-- Resetting Engine --\n");

	// kill music
	if (audiomixer) audiomixer->reset();

	// now, reset everything (order matters)
	world->reset();
	ucmachine->reset();
	// ObjectManager, Kernel have to be last, because they kill
	// all processes/objects
	objectmanager->reset();
	kernel->reset();
	palettemanager->resetTransforms();

	// Reset thet gumps
	desktopGump = 0;
	consoleGump = 0;
	gameMapGump = 0;
	scalerGump = 0;
	inverterGump = 0;

	textmodes.clear();

	// reset mouse cursor
	while (!cursors.empty()) cursors.pop();
	pushMouseCursor();

	kernel->addProcess(new JoystickCursorProcess(JOY1, 0, 1));

	timeOffset = -(sint32)Kernel::get_instance()->getFrameNum();
	inversion = 0;
	save_count = 0;
	has_cheated = false;

	con.Print(MM_INFO, "-- Engine Reset --\n");
}

void GUIApp::setupCoreGumps()
{
	con.Print(MM_INFO, "Setting up core game gumps...\n");

	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);

	con.Print(MM_INFO, "Creating Desktop...\n");
	desktopGump = new DesktopGump(0,0, dims.w, dims.h);
	desktopGump->InitGump(0);
	desktopGump->MakeFocus();

	con.Print(MM_INFO, "Creating ScalerGump...\n");
	scalerGump = new ScalerGump(0, 0, dims.w, dims.h);
	scalerGump->InitGump(0);

	Pentagram::Rect scaled_dims;
	scalerGump->GetDims(scaled_dims);

	con.Print(MM_INFO, "Creating Graphics Console...\n");
	consoleGump = new ConsoleGump(0, 0, dims.w, dims.h);
	consoleGump->InitGump(0);
	consoleGump->HideConsole();
	
	con.Print(MM_INFO, "Creating Inverter...\n");
	inverterGump = new InverterGump(0, 0, scaled_dims.w, scaled_dims.h);
	inverterGump->InitGump(0);

	con.Print(MM_INFO, "Creating GameMapGump...\n");
	gameMapGump = new GameMapGump(0, 0, scaled_dims.w, scaled_dims.h);
	gameMapGump->InitGump(0);


	// TODO: clean this up
	assert(desktopGump->getObjId() == 256);
	assert(scalerGump->getObjId() == 257);
	assert(consoleGump->getObjId() == 258);
	assert(inverterGump->getObjId() == 259);
	assert(gameMapGump->getObjId() == 260);


	for (uint16 i = 261; i < 384; ++i)
		objectmanager->reserveObjId(i);
}

bool GUIApp::newGame(const std::string &savegame)
{
	con.Print(MM_INFO, "Starting New Game...\n");

	resetEngine();

	setupCoreGumps();

	game->startGame();

	con.Print(MM_INFO, "Create Camera...\n");
	CameraProcess::SetCameraProcess(new CameraProcess(1)); // Follow Avatar

	con.Print(MM_INFO, "Create persistent Processes...\n");
	avatarMoverProcess = new AvatarMoverProcess();
	kernel->addProcess(avatarMoverProcess);

	kernel->addProcess(new HealProcess());

	kernel->addProcess(new SchedulerProcess());

	if (audiomixer) audiomixer->createProcesses();

//	av->teleport(40, 16240, 15240, 64); // central Tenebrae
//	av->teleport(3, 11391, 1727, 64); // docks, near gate
//	av->teleport(39, 16240, 15240, 64); // West Tenebrae
//	av->teleport(41, 12000, 15000, 64); // East Tenebrae
//	av->teleport(8, 14462, 15178, 48); // before entrance to Mythran's house
//	av->teleport(40, 13102,9474,48); // entrance to Mordea's throne room
//	av->teleport(54, 14783,5959,8); // shrine of the Ancient Ones; Hanoi
//	av->teleport(5, 5104,22464,48); // East road (tenebrae end)

	game->startInitialUsecode(savegame);

	settingman->set("lastSave", savegame);

	return true;
}

bool GUIApp::loadGame(std::string filename)
{
	con.Print(MM_INFO, "Loading...\n");

	IDataSource* ids = filesystem->ReadFile(filename);
	if (!ids) {
		Error("Can't load file", "Error Loading savegame " + filename);
		settingman->set("lastSave", "");
		return false;
	}

	Savegame* sg = new Savegame(ids);
	uint32 version = sg->getVersion();
	if (version == 0) {
		Error("Invalid or corrupt savegame", "Error Loading savegame " + filename);
		delete sg;
		settingman->set("lastSave", "");
		return false;
	}

	if (version == 1 || version > Pentagram::savegame_version) {
		char vstring[16];
		std::sprintf (vstring,"%i", version);
		Error(std::string("Unsupported savegame version (") + vstring + ")", "Error Loading savegame " + filename);
		delete sg;
		settingman->set("lastSave", "");
		return false;
	}
	IDataSource* ds;
	GameInfo saveinfo;
	ds = sg->getDataSource("GAME");
	bool ok = saveinfo.load(ds, version);

	if (!ok) {
		Error("Invalid or corrupt savegame: missing GameInfo", "Error Loading savegame " + filename);
		delete sg;
		return false;
	}

	if (!gameinfo->match(saveinfo)) {
		std::string message = "Game mismatch\n";
		            message+= "Running game: " + gameinfo->getPrintDetails()  + "\n";
		            message+= "Savegame    : " + saveinfo.getPrintDetails();

#ifdef DEBUG
		bool ignore;
		settingman->setDefault("ignore_savegame_mismatch", false);
		settingman->get("ignore_savegame_mismatch", ignore);

		if (!ignore)
		{
			Error(message, "Error Loading savegame " + filename);
			delete sg;
			return false;
		}
		perr << message << std::endl;
#else
		settingman->set("lastSave", "");
		Error(message,"Error Loading savegame " + filename);
		return false;
#endif
	}

	resetEngine();

	setupCoreGumps();

 	// and load everything back (order matters)
	bool totalok = true;

	std::string message;

	// UCSTRINGS, UCGLOBALS, UCLISTS don't depend on anything else,
	// so load these first
	ds = sg->getDataSource("UCSTRINGS");
	ok = ucmachine->loadStrings(ds, version);
	totalok &= ok;
	perr << "UCSTRINGS: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "UCSTRINGS: failed\n";
	delete ds;

	ds = sg->getDataSource("UCGLOBALS");
	ok = ucmachine->loadGlobals(ds, version);
	totalok &= ok;
	perr << "UCGLOBALS: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "UCGLOBALS: failed\n";
	delete ds;

	ds = sg->getDataSource("UCLISTS");
	ok = ucmachine->loadLists(ds, version);
	totalok &= ok;
	perr << "UCLISTS: " << (ok ? "ok" : "failed")<< std::endl;
	if (!ok) message += "UCLISTS: failed\n";
	delete ds;

	// KERNEL must be before OBJECTS, for the egghatcher
	// KERNEL must be before APP, for the avatarMoverProcess
	ds = sg->getDataSource("KERNEL");
	ok = kernel->load(ds, version);
	totalok &= ok;
	perr << "KERNEL: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "KERNEL: failed\n";
	delete ds;

	ds = sg->getDataSource("APP");
	ok = load(ds, version);
	totalok &= ok;
	perr << "APP: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "APP: failed\n";
	delete ds;

	// WORLD must be before OBJECTS, for the egghatcher
	ds = sg->getDataSource("WORLD");
	ok = world->load(ds, version);
	totalok &= ok;
	perr << "WORLD: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "WORLD: failed\n";
	delete ds;

	ds = sg->getDataSource("CURRENTMAP");
	ok = world->getCurrentMap()->load(ds, version);
	totalok &= ok;
	perr << "CURRENTMAP: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "CURRENTMAP: failed\n";
	delete ds;

	ds = sg->getDataSource("OBJECTS");
	ok = objectmanager->load(ds, version);
	totalok &= ok;
	perr << "OBJECTS: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "OBJECTS: failed\n";
	delete ds;

	ds = sg->getDataSource("MAPS");
	ok = world->loadMaps(ds, version);
	totalok &= ok;
	perr << "MAPS: " << (ok ? "ok" : "failed") << std::endl;
	if (!ok) message += "MAPS: failed\n";
	delete ds;

	if (!totalok) {
		Error(message, "Error Loading savegame " + filename, true);
		delete sg;
		return false;
	}

	pout << "Done" << std::endl;

	settingman->set("lastSave", filename);

	delete sg;
	return true;
}

void GUIApp::Error(std::string message, std::string title, bool exit_to_menu)
{
	if (title.empty()) title = exit_to_menu?"Fatal Game Error":"Error";

	perr << title << ": " << message << std::endl;
	
	error_message = message;
	error_title = title;

	if (exit_to_menu) 
	{
		change_gamename = "pentagram";
		Kernel::get_instance()->killProcesses(0,6,false);
	}
}

Gump* GUIApp::getGump(uint16 gumpid)
{
	return p_dynamic_cast<Gump*>(ObjectManager::get_instance()->
								 getObject(gumpid));
}

void GUIApp::addGump(Gump* gump)
{
	// TODO: At some point, this will have to _properly_ choose to
	// which 'layer' to add the gump: inverted, scaled or neither.

	assert(desktopGump);

	if (gump->IsOfType<ShapeViewerGump>() || gump->IsOfType<MiniMapGump>() ||
		gump->IsOfType<ConsoleGump>() || gump->IsOfType<ScalerGump>() ||
		gump->IsOfType<PentagramMenuGump>() || gump->IsOfType<MessageBoxGump>()// ||
		//(ttfoverrides && (gump->IsOfType<BarkGump>() ||
		//				  gump->IsOfType<AskGump>()))
		)
	{
//		pout << "adding to desktopgump: "; gump->dumpInfo();
		desktopGump->AddChild(gump);
	}
	else if (gump->IsOfType<GameMapGump>())
	{
//		pout << "adding to invertergump: "; gump->dumpInfo();
		inverterGump->AddChild(gump);
	}
	else if (gump->IsOfType<InverterGump>())
	{
//		pout << "adding to scalergump: "; gump->dumpInfo();
		scalerGump->AddChild(gump);
	}
	else if (gump->IsOfType<DesktopGump>())
	{
	}
	else
	{
//		pout << "adding to scalergump: "; gump->dumpInfo();
		scalerGump->AddChild(gump);
	}
}

uint32 GUIApp::getGameTimeInSeconds()
{
	// 1 second per every 30 frames
	return (Kernel::get_instance()->getFrameNum()+timeOffset)/30; // constant!
}


void GUIApp::save(ODataSource* ods)
{
	uint8 s = (avatarInStasis ? 1 : 0);
	ods->write1(s);

	sint32 absoluteTime = Kernel::get_instance()->getFrameNum()+timeOffset;
	ods->write4(static_cast<uint32>(absoluteTime));
	ods->write2(avatarMoverProcess->getPid());

	Pentagram::Palette *pal = PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game);
	for (int i = 0; i < 12; i++) ods->write2(pal->matrix[i]);
	ods->write2(pal->transform);

	ods->write2(static_cast<uint16>(inversion));

	ods->write4(save_count);

	uint8 c = (has_cheated ? 1 : 0);
	ods->write1(c);
}

bool GUIApp::load(IDataSource* ids, uint32 version)
{
	avatarInStasis = (ids->read1() != 0);

	// no gump should be moused over after load
	mouseOverGump = 0;

	sint32 absoluteTime = static_cast<sint32>(ids->read4());
	timeOffset = absoluteTime - Kernel::get_instance()->getFrameNum();

	uint16 amppid = ids->read2();
	avatarMoverProcess = p_dynamic_cast<AvatarMoverProcess*>(Kernel::get_instance()->getProcess(amppid));

	sint16 matrix[12];
	for (int i = 0; i < 12; i++)
		matrix[i] = ids->read2();

	PaletteManager::get_instance()->transformPalette(PaletteManager::Pal_Game, matrix);
	Pentagram::Palette *pal = PaletteManager::get_instance()->getPalette(PaletteManager::Pal_Game);
	pal->transform = static_cast<Pentagram::PalTransforms>(ids->read2());

	inversion = ids->read2();

	save_count = ids->read4();

	has_cheated = (ids->read1() != 0);

	return true;
}

//
// Console Commands
//

void GUIApp::ConCmd_saveGame(const Console::ArgvType &argv)
{
	if (argv.size()==1)
	{
		pout << "Usage: GUIApp::saveGame <filename>" << std::endl;
		return;
	}

	std::string filename = "@save/";
	filename += argv[1].c_str();
	GUIApp::get_instance()->saveGame(filename, argv[1]);
}

void GUIApp::ConCmd_loadGame(const Console::ArgvType &argv)
{
	if (argv.size()==1)
	{
		pout << "Usage: GUIApp::loadGame <filename>" << std::endl;
		return;
	}

	std::string filename = "@save/";
	filename += argv[1].c_str();
	GUIApp::get_instance()->loadGame(filename);
}

void GUIApp::ConCmd_newGame(const Console::ArgvType &argv)
{
	GUIApp::get_instance()->newGame(std::string());
}


void GUIApp::ConCmd_quit(const Console::ArgvType &argv)
{
	GUIApp::get_instance()->isRunning = false;
}

void GUIApp::ConCmd_drawRenderStats(const Console::ArgvType &argv)
{
	if (argv.size() == 1)
	{
		pout << "GUIApp::drawRenderStats = " << GUIApp::get_instance()->drawRenderStats << std::endl;
	}
	else
	{
		GUIApp::get_instance()->drawRenderStats = std::strtol(argv[1].c_str(), 0, 0) != 0;
	}
}

void GUIApp::ConCmd_engineStats(const Console::ArgvType &argv)
{
	Kernel::get_instance()->kernelStats();
	ObjectManager::get_instance()->objectStats();
	UCMachine::get_instance()->usecodeStats();
	World::get_instance()->worldStats();
}

void GUIApp::ConCmd_changeGame(const Console::ArgvType &argv)
{
	if (argv.size() == 1)
	{
		pout << "Current game is: " << GUIApp::get_instance()->gameinfo->name << std::endl;
	}
	else
	{
		GUIApp::get_instance()->changeGame(argv[1]);
	}
}

void GUIApp::ConCmd_listGames(const Console::ArgvType &argv)
{
	GUIApp *app = GUIApp::get_instance(); 
	std::vector<Pentagram::istring> games;
	games = app->settingman->listGames();
	std::vector<Pentagram::istring>::iterator iter;
	for (iter = games.begin(); iter != games.end(); ++iter) {
		Pentagram::istring game = *iter;
		GameInfo* info = app->getGameInfo(game);
		con.Printf(MM_INFO, "%s: ", game.c_str());
		if (info) {
			std::string details = info->getPrintDetails();
			con.Print(MM_INFO, details.c_str());
		} else {
			con.Print(MM_INFO, "(unknown)");
		}
		con.Print(MM_INFO, "\n");
	}
}

void GUIApp::ConCmd_setVideoMode(const Console::ArgvType &argv)
{
	int fullscreen = -1;
	
	//if (argv.size() == 4) {
	//	if (argv[3] == "fullscreen") fullscreen = 1;
	//	else fullscreen = 0;
	//} else 
	if (argv.size() != 3)
	{
		//pout << "Usage: GUIApp::setVidMode width height [fullscreen/windowed]" << std::endl;
		pout << "Usage: GUIApp::setVidMode width height" << std::endl;
		return;
	}

	GUIApp::get_instance()->changeVideoMode(strtol(argv[1].c_str(), 0, 0), strtol(argv[2].c_str(), 0, 0), fullscreen);
}

void GUIApp::ConCmd_toggleFullscreen(const Console::ArgvType &argv)
{
	GUIApp::get_instance()->changeVideoMode(-1, -1, -2);
}

void GUIApp::ConCmd_toggleAvatarInStasis(const Console::ArgvType &argv)
{
	GUIApp * g = GUIApp::get_instance();
	g->toggleAvatarInStasis();
	pout << "avatarInStasis = " << g->isAvatarInStasis() << std::endl;
}

void GUIApp::ConCmd_togglePaintEditorItems(const Console::ArgvType &argv)
{
	GUIApp * g = GUIApp::get_instance();
	g->togglePaintEditorItems();
	pout << "paintEditorItems = " << g->isPaintEditorItems() << std::endl;
}

void GUIApp::ConCmd_toggleShowTouchingItems(const Console::ArgvType &argv)
{
	GUIApp * g = GUIApp::get_instance();
	g->toggleShowTouchingItems();
	pout << "ShowTouchingItems = " << g->isShowTouchingItems() << std::endl;
}

void GUIApp::ConCmd_closeItemGumps(const Console::ArgvType &argv)
{
	GUIApp * g = GUIApp::get_instance();
	g->getDesktopGump()->CloseItemDependents();
}

void GUIApp::ConCmd_memberVar(const Console::ArgvType &argv)
{
	if (argv.size() == 1) {
		pout << "Usage: GUIApp::memberVar <member> [newvalue] [updateini]" << std::endl;
		return;
	}

	GUIApp * g = GUIApp::get_instance();

	// Set the pointer to the correct type
	bool *b = 0;
	int *i = 0;
	std::string *str = 0;
	Pentagram::istring *istr = 0;

	// ini entry name if supported
	const char *ini = 0;

	if (argv[1] == "frameLimit") { 
		b = &g->frameLimit;
		ini = "frameLimit";
	}
	else if (argv[1] == "frameSkip") { 
		b = &g->frameSkip;
		ini = "frameSkip";
	}
	else if (argv[1] == "interpolate") { 
		b = &g->interpolate;
		ini = "interpolate";
	}
	else {
		pout << "Unknown member: " << argv[1] << std::endl;
		return;
	}

	// Set the value
	if (argv.size() >= 3) {
		if (b) *b = (argv[2] == "yes" || argv[2] == "true");
		else if (istr) *istr = argv[2];
		else if (i) *i = std::strtol(argv[2].c_str(), 0, 0);
		else if (str) *str = argv[2];

		// Set config value
		if (argv.size() >= 4 && ini && *ini && (argv[3] == "yes" || argv[3] == "true"))
		{
			if (b) g->settingman->set(ini,*b);
			else if (istr) g->settingman->set(ini,*istr);
			else if (i) g->settingman->set(ini,*i);
			else if (str) g->settingman->set(ini,*str);
		}
	}

	// Print the value
	pout << "GuiApp::" << argv[1] << " = ";
	if (b) pout << ((*b)?"true":"false");
	else if (istr) pout << *istr;
	else if (i) pout << *i;
	else if (str) pout << *str;
	pout << std::endl;

	return;
}

//
// Intrinsics
//

uint32 GUIApp::I_makeAvatarACheater(const uint8* /*args*/,
									unsigned int /*argsize*/)
{
	GUIApp::get_instance()->makeCheater();
	return 0;
}

uint32 GUIApp::I_getCurrentTimerTick(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// number of ticks of a 60Hz timer, with the default animrate of 30Hz
	return Kernel::get_instance()->getFrameNum()*2;
}

uint32 GUIApp::I_setAvatarInStasis(const uint8* args, unsigned int /*argsize*/)
{
	ARG_SINT16(stasis);
	get_instance()->setAvatarInStasis(stasis!=0);
	return 0;
}

uint32 GUIApp::I_getAvatarInStasis(const uint8* /*args*/, unsigned int /*argsize*/)
{
	if (get_instance()->avatarInStasis)
		return 1;
	else
		return 0;
}

uint32 GUIApp::I_getTimeInGameHours(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// 900 seconds per game hour
	return get_instance()->getGameTimeInSeconds() / 900;
}

uint32 GUIApp::I_getTimeInMinutes(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// 60 seconds per minute
	return get_instance()->getGameTimeInSeconds() / 60;
}

uint32 GUIApp::I_getTimeInSeconds(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	return get_instance()->getGameTimeInSeconds();
}

uint32 GUIApp::I_setTimeInGameHours(const uint8* args,
										unsigned int /*argsize*/)
{
	ARG_UINT16(newhour);

	// 1 game hour per every 27000 frames
	sint32	absolute = newhour*27000;
	get_instance()->timeOffset = absolute-Kernel::get_instance()->getFrameNum();

	return 0;
}

uint32 GUIApp::I_closeItemGumps(const uint8* args, unsigned int /*argsize*/)
{
	GUIApp* g = GUIApp::get_instance();
	g->getDesktopGump()->CloseItemDependents();

	return 0;
}
