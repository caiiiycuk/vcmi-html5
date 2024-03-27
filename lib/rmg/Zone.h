/*
 * Zone.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../GameConstants.h"
#include "float3.h"
#include "../int3.h"
#include "../CRandomGenerator.h"
#include "CRmgTemplate.h"
#include "RmgArea.h"
#include "RmgPath.h"
#include "RmgObject.h"
#include "modificators/Modificator.h"

//uncomment to generate dumps afger every step of map generation
//#define RMG_DUMP

VCMI_LIB_NAMESPACE_BEGIN

class RmgMap;
class CMapGenerator;
class Modificator;
class CRandomGenerator;

extern const std::function<bool(const int3 &)> AREA_NO_FILTER;

typedef std::list<std::shared_ptr<Modificator>> TModificators;

template<typename T>
class ThreadSafeProxy
{
public:
	ThreadSafeProxy(T& resource, boost::recursive_mutex& mutex)
		: resourceRef(resource), lock(mutex) {}

	T* operator->() { return &resourceRef; }
	const T* operator->() const { return &resourceRef; }
	T& operator*() { return resourceRef; }
	const T& operator*() const { return resourceRef; }
	T& get() {return resourceRef;}
	const T& get() const {return resourceRef;}


	T operator+(const T & other)
	{
		return resourceRef + other;
	}

	T operator+(ThreadSafeProxy<T> & other)
	{
		return get() + other.get();
	}

private:
	T& resourceRef;
	std::lock_guard<boost::recursive_mutex> lock;
};

class Zone : public rmg::ZoneOptions
{
public:
	Zone(RmgMap & map, CMapGenerator & generator, CRandomGenerator & rand);
	Zone(const Zone &) = delete;
	
	void setOptions(const rmg::ZoneOptions & options);
	bool isUnderground() const;
	
	float3 getCenter() const;
	void setCenter(const float3 &f);
	int3 getPos() const;
	void setPos(const int3 &pos);
	
	ThreadSafeProxy<rmg::Area> area(); 
	ThreadSafeProxy<const rmg::Area> area() const;
	ThreadSafeProxy<rmg::Area> areaPossible();
	ThreadSafeProxy<const rmg::Area> areaPossible() const;
	ThreadSafeProxy<rmg::Area> freePaths();
	ThreadSafeProxy<const rmg::Area> freePaths() const;
	ThreadSafeProxy<rmg::Area> areaUsed();
	ThreadSafeProxy<const rmg::Area> areaUsed() const;

	void initFreeTiles();
	void clearTiles();
	void fractalize();
	
	FactionID getTownType() const;
	void setTownType(FactionID town);
	TerrainId getTerrainType() const;
	void setTerrainType(TerrainId terrain);
		
	void connectPath(const rmg::Path & path);
	rmg::Path searchPath(const rmg::Area & src, bool onlyStraight, const std::function<bool(const int3 &)> & areafilter = AREA_NO_FILTER) const;
	rmg::Path searchPath(const int3 & src, bool onlyStraight, const std::function<bool(const int3 &)> & areafilter = AREA_NO_FILTER) const;
	rmg::Path searchPath(const rmg::Area & src, bool onlyStraight, const rmg::Area & searchArea) const;

	TModificators getModificators();

	template<class T>
	T* getModificator()
	{
		for(auto & m : modificators)
			if(auto * mm = dynamic_cast<T*>(m.get()))
				return mm;
		return nullptr;
	}
	
	template<class T>
	void addModificator()
	{
		modificators.emplace_back(new T(*this, map, generator));
	}
	
	void initModificators();
	
	CRandomGenerator & getRand();
public:
	mutable boost::recursive_mutex areaMutex;
	using Lock = boost::unique_lock<boost::recursive_mutex>;
	
protected:
	CMapGenerator & generator;
	CRandomGenerator rand;
	RmgMap & map;
	TModificators modificators;
	bool finished;
	
	//placement info
	int3 pos;
	float3 center;
	rmg::Area dArea; //irregular area assined to zone
	rmg::Area dAreaPossible;
	rmg::Area dAreaFree; //core paths of free tiles that all other objects will be linked to
	rmg::Area dAreaUsed;
	std::vector<int3> possibleQuestArtifactPos;
	
	//template info
	FactionID townType;
	TerrainId terrainType;
};

VCMI_LIB_NAMESPACE_END
