/*
 OpenLieroX
 
 hide and seek gamemode
 
 created 2009-02-09
 code under LGPL
 */

#include <iostream>
#include "CHideAndSeek.h"
#include "CWorm.h"
#include "Options.h"
#include "Consts.h"
#include "CServer.h"
#include "CClient.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "CMap.h"
#include "LieroX.h"

CHideAndSeek::CHideAndSeek(GameServer* server, CWorm* worms)
{
	cServer = server;
	cWorms = worms;
}

CHideAndSeek::~CHideAndSeek()
{
}

void CHideAndSeek::PrepareGame()
{
	fGameStart = tLX->fCurTime;
	GenerateTimes();
	if(tLXOptions->tGameInfo.fTimeLimit > 0)
		fGameLength = tLXOptions->tGameInfo.fTimeLimit * 60;
	for(int i = 0; i < MAX_WORMS; i++) {
		fLastAlert[i] = 0;
		bVisible[i] = false;
	}
}

void CHideAndSeek::PrepareWorm(CWorm* worm)
{
	// TODO: move to network texts
	std::string teamhint[2] = {
		"You are a hider, you have to run away from the seekers who are red. You have to hide for "
			+ itoa((int)fGameLength) + " seconds.",
		"You are a seeker, you have to find and catch the hiders. You have to catch the hiders before "
			+ itoa((int)fGameLength) + " seconds are up."
	};
	worm->setLives(0);
	// Gameplay hints
	worm->getClient()->getNetEngine()->SendText(teamhint[worm->getTeam()], TXT_NORMAL);
}

bool CHideAndSeek::Spawn(CWorm* worm, CVec pos)
{
	pos = cServer->FindSpot();
	worm->Spawn(pos);
	// Worms only spawn visible to their own team
	for(int i = 0; i < MAX_WORMS; i++)
		if(cWorms[i].isUsed() && cWorms[i].getTeam() == worm->getTeam())
			cWorms[i].getClient()->getNetEngine()->SendSpawnWorm(worm, pos);
	return false;
}

void CHideAndSeek::Kill(CWorm* victim, CWorm* killer)
{
	if(killer->getTeam() == SEEKER && killer != victim) {
		cServer->SendGlobalText(killer->getName() + " caught " + victim->getName(), TXT_NORMAL);
		killer->AddKill();
	}
	victim->Kill();
}

bool CHideAndSeek::Shoot(CWorm* worm)
{
	return false;
}

void CHideAndSeek::Drop(CWorm* worm)
{
}

void CHideAndSeek::Simulate()
{
	float GameTime = tLX->fCurTime - fGameStart;
	// Game time up
	if(GameTime > fGameLength + fHideLength) {
		for(int i = 0; i < MAX_WORMS; i++)
			if(cWorms[i].isUsed() && cWorms[i].getLives() != WRM_OUT && cWorms[i].getTeam() == SEEKER)
				cServer->killWorm(i, i, cWorms[i].getLives() + 1);
		return;
	}
	// Hiders have some time free from being caught and seen
	if(GameTime < fHideLength)
		return;
	// Check if any of the worms can see eachother
	int i, j;
	for(i = 0; i < MAX_WORMS; i++) {
		if(!cWorms[i].isUsed() || cWorms[i].getLives() == WRM_OUT)
			continue;
		// Hide the worm if the alert time is up
		if(fLastAlert[i] + fAlertLength < GameTime)
			Hide(&cWorms[i]);
		for(j = 0; j < MAX_WORMS; j++) {
			if(!cWorms[j].isUsed() || cWorms[j].getLives() == WRM_OUT)
				continue;
			if(cWorms[j].getTeam() == cWorms[i].getTeam())
				continue;
			if(CanSee(&cWorms[i], &cWorms[j]))
				Show(&cWorms[j]);
			// Catch the hiders if they are within 10 pixels
			if(cWorms[i].getTeam() == SEEKER && cWorms[j].getTeam() == HIDER)
				if((cWorms[i].getPos() - cWorms[j].getPos()).GetLength() < 10)
					cServer->killWorm(cWorms[j].getID(), cWorms[i].getID(), 0);
		}
	}
}

bool CHideAndSeek::CheckGame()
{
	// Empty games, no need to check anything?
	if(tLXOptions->tGameInfo.features[FT_AllowEmptyGames])
		return false;
	
	// In game?
	if (!cServer || cServer->getState() == SVS_LOBBY)
		return false;

	int worms[2] = { 0, 0 };
	int winners = -1;
	static const std::string teamname[2] = { "hiding", "seeking" };

	for(int i = 0; i < MAX_WORMS; i++)
		if(cWorms[i].isUsed() && cWorms[i].getLives() != WRM_OUT)
			worms[cWorms[i].getTeam()]++;
	if(worms[0] == 0)
		winners = SEEKER;
	else if(worms[1] == 0)
		winners = HIDER;
	if(winners != -1) {
		if(networkTexts->sTeamHasWon != "<none>")
			cServer->SendGlobalText((replacemax(networkTexts->sTeamHasWon, "<team>",
				teamname[winners], 1)), TXT_NORMAL);
		return true;
	}
	return false;
}

int CHideAndSeek::GameType()
{
	return GMT_TEAMS;
}

int CHideAndSeek::GameTeams()
{
	return 2;
}

int CHideAndSeek::Winner()
{
	return 0;
}

void CHideAndSeek::Show(CWorm* worm)
{
	fLastAlert[worm->getID()] = tLX->fCurTime - fGameStart;
	if(bVisible[worm->getID()])
		return;
	bVisible[worm->getID()] = true;

	if(worm->getTeam() == HIDER)
		worm->getClient()->getNetEngine()->SendText("You are visible to the seekers, run!", TXT_NORMAL);
	else
		worm->getClient()->getNetEngine()->SendText("You are visible to the hiders.", TXT_NORMAL);
	for(int i = 0; i < MAX_WORMS; i++) {
		if(!cWorms[i].isUsed() || cWorms[i].getTeam() == worm->getTeam())
			continue;
		cWorms[i].getClient()->getNetEngine()->SendSpawnWorm(worm, worm->getPos());
		cWorms[i].getClient()->getNetEngine()->SendText(worm->getName() + " is visible!", TXT_NORMAL);
	}
}

void CHideAndSeek::Hide(CWorm* worm)
{
	if(!bVisible[worm->getID()])
		return;
	bVisible[worm->getID()] = false;

	worm->getClient()->getNetEngine()->SendText("You are invisible again!", TXT_NORMAL);
	for(int i = 0; i < MAX_WORMS; i++) {
		if(!cWorms[i].isUsed() || cWorms[i].getTeam() == worm->getTeam())
			continue;
		cWorms[i].getClient()->getNetEngine()->SendWormDied(worm);
		cWorms[i].getClient()->getNetEngine()->SendText(worm->getName() + " is hiding!", TXT_NORMAL);
	}
}

bool CHideAndSeek::CanSee(CWorm* worm1, CWorm* worm2)
{
	float length;
	int type;
	CVec dist;
	worm1->traceLine(worm2->getPos(), &length, &type, 1);
	dist = worm1->getPos() - worm2->getPos();
	// Seekers can see 125px
	if(worm1->getTeam() == SEEKER)
		return type & PX_EMPTY && (dist.GetLength() < 125);
	// Hiders can see 175px or 25px through walls
	else {
		if(type & PX_EMPTY)
			return dist.GetLength() < 175;
		else
			return dist.GetLength() < 25;
	}
}

void CHideAndSeek::GenerateTimes()
{
	CMap* cMap = cServer->getMap();
	int volume = cMap->GetWidth() * cMap->GetHeight();
	enum { SMALL = 1, MEDIUM, LARGE, XLARGE } size;
	// Decide on the level size
	if(volume < 500 * 400)
		size = SMALL;
	else if(volume < 700 * 600)
		size = MEDIUM;
	else if(volume < 900 * 800)
		size = LARGE;
	else
		size = XLARGE;
	// Calculate the ratio of hiders to seekers
	float ratio = 1.0f;
	int worms[2] = { 0, 0 };
	for(int i = 0; i < MAX_WORMS; i++)
		if(cWorms[i].isUsed())
			worms[cWorms[i].getTeam()]++;
	if(worms[0] != 0 && worms[1] != 0)
		ratio = worms[0] / worms[1];

	// TODO: Is this actually any good? 
	fGameLength = (int)(45 * size * ratio);
	fHideLength = 15;
	fAlertLength = 10;
}

