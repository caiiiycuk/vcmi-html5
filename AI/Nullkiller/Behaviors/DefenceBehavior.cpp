/*
* DefenceBehavior.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "DefenceBehavior.h"
#include "../VCAI.h"
#include "../Engine/Nullkiller.h"
#include "../AIUtility.h"
#include "../Goals/BuyArmy.h"
#include "../Goals/ExecuteHeroChain.h"
#include "../Goals/RecruitHero.h"
#include "../Goals/DismissHero.h"
#include "../Goals/ExchangeSwapTownHeroes.h"
#include "lib/mapping/CMap.h" //for victory conditions
#include "lib/CPathfinder.h"

extern boost::thread_specific_ptr<CCallback> cb;
extern boost::thread_specific_ptr<VCAI> ai;

using namespace Goals;

std::string DefenceBehavior::toString() const
{
	return "Defend towns";
}

Goals::TGoalVec DefenceBehavior::decompose() const
{
	Goals::TGoalVec tasks;
		
	for(auto town : cb->getTownsInfo())
	{
		evaluateDefence(tasks, town);
	}

	return tasks;
}

uint64_t townArmyIncome(const CGTownInstance * town)
{
	uint64_t result = 0;

	for(auto creatureInfo : town->creatures)
	{
		if(creatureInfo.second.empty())
			continue;

		auto creature = creatureInfo.second.back().toCreature();
		result += creature->AIValue * town->getGrowthInfo(creature->level).totalGrowth();
	}

	return result;
}

void DefenceBehavior::evaluateDefence(Goals::TGoalVec & tasks, const CGTownInstance * town) const
{
	auto basicPriority = 0.3f + std::sqrt(townArmyIncome(town) / 40000.0f)
		+ town->dailyIncome()[Res::GOLD] / 10000.0f;

	logAi->debug("Evaluating defence for %s, basic priority %f", town->name, basicPriority);

	auto treatNode = ai->nullkiller->dangerHitMap->getObjectTreat(town);
	auto treats = { treatNode.fastestDanger, treatNode.maximumDanger };

	if(!treatNode.fastestDanger.hero)
	{
		logAi->debug("No treat found for town %s", town->name);

		return;
	}

	int dayOfWeek = cb->getDate(Date::DAY_OF_WEEK);

	if(town->garrisonHero)
	{
		if(!ai->nullkiller->isHeroLocked(town->garrisonHero.get()))
		{
			if(!town->visitingHero)
			{
				tasks.push_back(Goals::sptr(Goals::ExchangeSwapTownHeroes(town, nullptr).setpriority(5)));
			}

			return;
		}

		logAi->debug(
			"Hero %s in garrison of town %s is suposed to defend the town",
			town->garrisonHero->name,
			town->name);

		return;
	}
	
	uint64_t reinforcement = ai->nullkiller->armyManager->howManyReinforcementsCanBuy(town->getUpperArmy(), town);

	if(reinforcement)
	{
		logAi->debug("Town %s can buy defence army %lld", town->name, reinforcement);
		tasks.push_back(Goals::sptr(Goals::BuyArmy(town, reinforcement).setpriority(0.5f)));
	}

	auto paths = ai->nullkiller->pathfinder->getPathInfo(town->visitablePos());

	for(auto & treat : treats)
	{
		logAi->debug(
			"Town %s has treat %lld in %s turns, hero: %s",
			town->name,
			treat.danger,
			std::to_string(treat.turn),
			treat.hero->name);

		bool treatIsUnderControl = false;

		for(AIPath & path : paths)
		{
			if(path.getHeroStrength() > treat.danger)
			{
				if(path.turn() <= treat.turn && dayOfWeek + treat.turn < 6 && isSafeToVisit(path.targetHero, path.heroArmy, treat.danger)
					|| path.exchangeCount == 1 && path.turn() < treat.turn
					|| path.turn() < treat.turn - 1
					|| path.turn() < treat.turn && treat.turn >= 2)
				{
					logAi->debug(
						"Hero %s can eliminate danger for town %s using path %s.",
						path.targetHero->name,
						town->name,
						path.toString());

					treatIsUnderControl = true;
					break;
				}
			}
		}

		if(treatIsUnderControl)
			continue;

		if(!town->visitingHero
			&& town->hasBuilt(BuildingID::TAVERN)
			&& cb->getResourceAmount(Res::GOLD) > GameConstants::HERO_GOLD_COST)
		{
			auto heroesInTavern = cb->getAvailableHeroes(town);

			for(auto hero : heroesInTavern)
			{
				if(hero->getTotalStrength() > treat.danger)
				{
					auto myHeroes = cb->getHeroesInfo();

					if(cb->getHeroesInfo().size() < ALLOWED_ROAMING_HEROES)
					{
						logAi->debug("Hero %s can be recruited to defend %s", hero->name, town->name);
						tasks.push_back(Goals::sptr(Goals::RecruitHero(town, hero).setpriority(1)));
						continue;
					}
					else
					{
						const CGHeroInstance * weakestHero = nullptr;

						for(auto existingHero : myHeroes)
						{
							if(ai->nullkiller->isHeroLocked(existingHero)
								|| existingHero->getArmyStrength() > hero->getArmyStrength()
								|| ai->nullkiller->heroManager->getHeroRole(existingHero) == HeroRole::MAIN
								|| existingHero->movement
								|| existingHero->artifactsWorn.size() > (existingHero->hasSpellbook() ? 2 : 1))
								continue;

							if(!weakestHero || weakestHero->getFightingStrength() > existingHero->getFightingStrength())
							{
								weakestHero = existingHero;
							}

							if(weakestHero)
							{
								tasks.push_back(Goals::sptr(Goals::DismissHero(weakestHero)));
							}
						}
					}
				}
			}
		}

		if(paths.empty())
		{
			logAi->debug("No ways to defend town %s", town->name);

			continue;
		}

		std::vector<Goals::ExecuteHeroChain> pathsToDefend;
		std::map<const CGHeroInstance *, std::vector<AIPath>> defferedPaths;

		for(AIPath & path : paths)
		{
#if AI_TRACE_LEVEL >= 1
			logAi->trace(
				"Hero %s can defend town with force %lld in %s turns, cost: %f, path: %s",
				path.targetHero->name,
				path.getHeroStrength(),
				std::to_string(path.turn()),
				path.movementCost(),
				path.toString());
#endif
			if(path.turn() <= treat.turn - 2)
			{
				logAi->trace("Deffer defence of %s by %s because he has enough time to rich the town next trun",
					town->name,
					path.targetHero->name);

				defferedPaths[path.targetHero].push_back(path);

				continue;
			}

			float priority = basicPriority * std::min(SAFE_ATTACK_CONSTANT, (float)path.getHeroStrength() / treat.danger) 
				- treat.turn * 0.2f;

			if(treat.turn < path.turn())
				priority /= (path.turn() - treat.turn) * 2;

			if(path.targetHero == town->visitingHero && path.exchangeCount == 1)
			{
#if AI_TRACE_LEVEL >= 1
				logAi->trace("Put %s to garrison of town %s with priority %f",
					path.targetHero->name,
					town->name,
					priority);
#endif

				tasks.push_back(Goals::sptr(Goals::ExchangeSwapTownHeroes(town, town->visitingHero.get(), HeroLockedReason::DEFENCE).setpriority(priority)));

				continue;
			}
				
			if(treat.turn == 0 || path.turn() <= treat.turn && path.getHeroStrength() * SAFE_ATTACK_CONSTANT >= treat.danger)
			{
				if(ai->nullkiller->arePathHeroesLocked(path))
				{
#if AI_TRACE_LEVEL >= 1
					logAi->trace("Can not move %s to defend town %s with priority %f. Path is locked.",
						path.targetHero->name,
						town->name,
						priority);

#endif
					continue;
				}

				pathsToDefend.push_back(Goals::ExecuteHeroChain(path, town).setpriority(priority));
			}
		}

		for(Goals::ExecuteHeroChain & chain : pathsToDefend)
		{
			auto path = chain.getPath();

			for(AIPath & defferedPath : defferedPaths[path.targetHero])
			{
				if(defferedPath.getHeroStrength() >= path.getHeroStrength()
					&& defferedPath.turn() <= path.turn())
				{
					continue;
				}
			}

#if AI_TRACE_LEVEL >= 1
			logAi->trace("Move %s to defend town %s with priority %f",
				path.targetHero->name,
				town->name,
				chain.priority);
#endif

			tasks.push_back(Goals::sptr(chain));
		}
	}

	logAi->debug("Found %d tasks", tasks.size());
}