/*
* RecruitHeroBehavior.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "RecruitHeroBehavior.h"
#include "../AIGateway.h"
#include "../AIUtility.h"
#include "../Goals/RecruitHero.h"
#include "../Goals/ExecuteHeroChain.h"

namespace NKAI
{

using namespace Goals;

std::string RecruitHeroBehavior::toString() const
{
	return "Recruit hero";
}

Goals::TGoalVec RecruitHeroBehavior::decompose(const Nullkiller * ai) const
{
	Goals::TGoalVec tasks;
	auto towns = ai->cb->getTownsInfo();

	auto ourHeroes = ai->heroManager->getHeroRoles();
	auto minScoreToHireMain = std::numeric_limits<float>::max();

	for(auto hero : ourHeroes)
	{
		if(hero.second != HeroRole::MAIN)
			continue;

		auto newScore = ai->heroManager->evaluateHero(hero.first.get());

		if(minScoreToHireMain > newScore)
		{
			// weakest main hero score
			minScoreToHireMain = newScore;
		}
	}
	// If we don't have any heros we might want to lower our expectations.
	if (ourHeroes.empty())
		minScoreToHireMain = 0;

	for(auto town : towns)
	{
		if(ai->heroManager->canRecruitHero(town))
		{
			auto availableHeroes = ai->cb->getAvailableHeroes(town);

			//TODO: Prioritize non-main-heros too by cost of their units and whether their units fit to the current town
			for(auto hero : availableHeroes)
			{
				auto score = ai->heroManager->evaluateHero(hero);
				if(score > minScoreToHireMain || hero->getArmyCost() > GameConstants::HERO_GOLD_COST)
				{
					tasks.push_back(Goals::sptr(Goals::RecruitHero(town, hero).setpriority(200)));
					break;
				}
			}

			if(ai->cb->getHeroesInfo().size() < ai->cb->getTownsInfo().size() + 1
				|| (ai->getFreeResources()[EGameResID::GOLD] > 10000 && !ai->buildAnalyzer->isGoldPressureHigh()))
			{
				tasks.push_back(Goals::sptr(Goals::RecruitHero(town).setpriority(3)));
			}
		}
	}

	return tasks;
}

}
