/*
* ObjectGraph.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "ObjectGraph.h"
#include "AIPathfinderConfig.h"
#include "../../../CCallback.h"
#include "../../../lib/mapping/CMap.h"
#include "../Engine/Nullkiller.h"

namespace NKAI
{

void ObjectGraph::updateGraph(const Nullkiller * ai)
{
	auto cb = ai->cb;
	auto mapSize = cb->getMapSize();

	std::map<const CGHeroInstance *, HeroRole> actors;
	std::map<const CGHeroInstance *, const CGObjectInstance *> actorObjectMap;

	auto addObjectActor = [&](const CGObjectInstance * obj)
	{
		auto objectActor = new CGHeroInstance(obj->cb);
		CRandomGenerator rng;
		auto visitablePos = obj->visitablePos();

		objectActor->setOwner(ai->playerID); // lets avoid having multiple colors
		objectActor->initHero(rng, static_cast<HeroTypeID>(0));
		objectActor->pos = objectActor->convertFromVisitablePos(visitablePos);
		objectActor->initObj(rng);

		actorObjectMap[objectActor] = obj;
		actors[objectActor] = obj->ID == Obj::TOWN ?  HeroRole::MAIN : HeroRole::SCOUT;
		addObject(obj);
	};

	for(auto obj : ai->memory->visitableObjs)
	{
		if(obj && obj->isVisitable() && obj->ID != Obj::HERO)
		{
			addObjectActor(obj);
		}
	}

	for(auto town : cb->getTownsInfo())
	{
		addObjectActor(town);
	}

	PathfinderSettings ps;
	
	ps.mainTurnDistanceLimit = 5;
	ps.scoutTurnDistanceLimit = 1;
	ps.allowBypassObjects = false;

	ai->pathfinder->updatePaths(actors, ps);

	foreach_tile_pos(cb.get(), [&](const CPlayerSpecificInfoCallback * cb, const int3 & pos)
		{
			auto paths = ai->pathfinder->getPathInfo(pos);

			for(AIPath & path1 : paths)
			{
				for(AIPath & path2 : paths)
				{
					if(path1.targetHero == path2.targetHero)
						continue;

					auto obj1 = actorObjectMap[path1.targetHero];
					auto obj2 = actorObjectMap[path2.targetHero];

					nodes[obj1->visitablePos()].connections[obj2->visitablePos()].update(
						path1.movementCost() + path2.movementCost(),
						path1.getPathDanger() + path2.getPathDanger());
				}
			}
		});

	for(auto h : actors)
	{
		delete h.first;
	}
}

void ObjectGraph::addObject(const CGObjectInstance * obj)
{
	nodes[obj->visitablePos()].init(obj);
}

void ObjectGraph::connectHeroes(const Nullkiller * ai)
{
	for(auto obj : ai->memory->visitableObjs)
	{
		if(obj && obj->ID == Obj::HERO)
		{
			addObject(obj);
		}
	}

	for(auto node : nodes)
	{
		auto pos = node.first;
		auto paths = ai->pathfinder->getPathInfo(pos);

		for(AIPath & path : paths)
		{
			auto heroPos = path.targetHero->visitablePos();

			nodes[pos].connections[heroPos].update(
				path.movementCost(),
				path.getPathDanger());

			nodes[heroPos].connections[pos].update(
				path.movementCost(),
				path.getPathDanger());
		}
	}
}

bool GraphNodeComparer::operator()(const GraphPathNodePointer & lhs, const GraphPathNodePointer & rhs) const
{
	return pathNodes.at(lhs.coord)[lhs.nodeType].cost > pathNodes.at(rhs.coord)[rhs.nodeType].cost;
}

void GraphPaths::calculatePaths(const CGHeroInstance * targetHero, const Nullkiller * ai)
{
	graph = *ai->baseGraph;
	graph.connectHeroes(ai);

	pathNodes.clear();

	GraphNodeComparer cmp(pathNodes);
	GraphPathNode::TFibHeap pq(cmp);

	pathNodes[targetHero->visitablePos()][GrapthPathNodeType::NORMAL].cost = 0;
	pq.emplace(GraphPathNodePointer(targetHero->visitablePos(), GrapthPathNodeType::NORMAL));

	while(!pq.empty())
	{
		GraphPathNodePointer pos = pq.top();
		pq.pop();

		auto & node = getNode(pos);

		node.isInQueue = false;

		graph.iterateConnections(pos.coord, [&](int3 target, ObjectLink o)
			{
				auto targetPointer = GraphPathNodePointer(target, pos.nodeType);
				auto & targetNode = getNode(targetPointer);

				if(targetNode.tryUpdate(pos, node, o))
				{
					if(targetNode.isInQueue)
					{
						pq.increase(targetNode.handle);
					}
					else
					{
						targetNode.handle = pq.emplace(targetPointer);
						targetNode.isInQueue = true;
					}
				}
			});
	}
}

void GraphPaths::dumpToLog() const
{
	for(auto & tile : pathNodes)
	{
		for(auto & node : tile.second)
		{
			if(!node.previous.valid())
				continue;

			logAi->trace(
				"%s -> %s: %f !%d",
				node.previous.coord.toString(),
				tile.first.toString(),
				node.cost,
				node.danger);
		}
	}
}

bool GraphPathNode::tryUpdate(const GraphPathNodePointer & pos, const GraphPathNode & prev, const ObjectLink & link)
{
	auto newCost = prev.cost + link.cost;

	if(newCost < cost)
	{
		previous = pos;
		danger = prev.danger + link.danger;
		cost = newCost;

		return true;
	}

	return false;
}

void GraphPaths::addChainInfo(std::vector<AIPath> & paths, int3 tile, const CGHeroInstance * hero, const Nullkiller * ai) const
{
	auto nodes = pathNodes.find(tile);

	if(nodes == pathNodes.end())
		return;

	for(auto & node : (*nodes).second)
	{
		if(!node.reachable())
			continue;

		std::vector<int3> tilesToPass;

		uint64_t danger = node.danger;
		float cost = node.cost;
		bool allowBattle = false;

		auto current = GraphPathNodePointer(nodes->first, node.nodeType);

		while(true)
		{
			auto currentTile = pathNodes.find(current.coord);

			if(currentTile == pathNodes.end())
				break;

			auto currentNode = currentTile->second[current.nodeType];

			allowBattle = allowBattle || currentNode.nodeType == GrapthPathNodeType::BATTLE;
			vstd::amax(danger, currentNode.danger);
			vstd::amax(cost, currentNode.cost);

			tilesToPass.push_back(current.coord);

			if(currentNode.cost < 2)
				break;

			current = currentNode.previous;
		}

		if(tilesToPass.empty())
			continue;

		auto entryPaths = ai->pathfinder->getPathInfo(tilesToPass.back());

		for(auto & path : entryPaths)
		{
			if(path.targetHero != hero)
				continue;

			for(auto graphTile = tilesToPass.rbegin(); graphTile != tilesToPass.rend(); graphTile++)
			{
				AIPathNodeInfo n;

				n.coord = *graphTile;
				n.cost = cost;
				n.turns = static_cast<ui8>(cost) + 1; // just in case lets select worst scenario
				n.danger = danger;
				n.targetHero = hero;

				for(auto & node : path.nodes)
				{
					node.parentIndex++;
				}

				path.nodes.insert(path.nodes.begin(), n);
			}

			path.armyLoss += ai->pathfinder->getStorage()->evaluateArmyLoss(path.targetHero, path.heroArmy->getArmyStrength(), danger);
			path.targetObjectDanger = ai->pathfinder->getStorage()->evaluateDanger(tile, path.targetHero, !allowBattle);
			path.targetObjectArmyLoss = ai->pathfinder->getStorage()->evaluateArmyLoss(path.targetHero, path.heroArmy->getArmyStrength(), path.targetObjectDanger);

			paths.push_back(path);
		}
	}
}

}
