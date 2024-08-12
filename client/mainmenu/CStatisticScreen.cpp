/*
 * CStatisticScreen.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "CStatisticScreen.h"
#include "../CGameInfo.h"

#include "../gui/CGuiHandler.h"
#include "../gui/WindowHandler.h"
#include "../gui/Shortcut.h"

#include "../render/Graphics.h"

#include "../widgets/Images.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/TextControls.h"
#include "../widgets/Buttons.h"
#include "../windows/InfoWindows.h"

#include "../../lib/gameState/GameStatistics.h"
#include "../../lib/texts/CGeneralTextHandler.h"

#include <vstd/DateUtils.h>

CStatisticScreen::CStatisticScreen(StatisticDataSet stat)
	: CWindowObject(BORDERED), statistic(stat)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;
	pos = center(Rect(0, 0, 800, 600));
	filledBackground = std::make_shared<FilledTexturePlayerColored>(ImagePath::builtin("DiBoxBck"), Rect(0, 0, pos.w, pos.h));
	filledBackground->setPlayerColor(PlayerColor(1));

	auto contentArea = Rect(10, 40, 780, 510);
	layout.push_back(std::make_shared<CLabel>(400, 20, FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW, CGI->generaltexth->translate("vcmi.statisticWindow.statistic")));
	layout.push_back(std::make_shared<TransparentFilledRectangle>(contentArea, ColorRGBA(0, 0, 0, 64), ColorRGBA(64, 80, 128, 255), 1));
	layout.push_back(std::make_shared<CButton>(Point(725, 564), AnimationPath::builtin("MUBCHCK"), CButton::tooltip(), [this](){ close(); }, EShortcut::GLOBAL_ACCEPT));

	buttonCsvSave = std::make_shared<CToggleButton>(Point(10, 564), AnimationPath::builtin("GSPBUT2"), CButton::tooltip(), [this](bool on){
		std::string path = statistic.writeCsv();
		CInfoWindow::showInfoDialog(CGI->generaltexth->translate("vcmi.statisticWindow.csvSaved") + "\n\n" + path, {});
	});
	buttonCsvSave->setTextOverlay(CGI->generaltexth->translate("vcmi.statisticWindow.csvSave"), EFonts::FONT_SMALL, Colors::YELLOW);

	auto plotData = extractData(stat, [](StatisticDataSetEntry val){ return val.resources[EGameResID::GOLD]; });
	chart = std::make_shared<LineChart>(contentArea.resize(-5), "test title", plotData);
}

std::map<ColorRGBA, std::vector<float>> CStatisticScreen::extractData(StatisticDataSet stat, std::function<float(StatisticDataSetEntry val)> selector)
{
	auto tmpData = stat.data;
	std::sort(tmpData.begin(), tmpData.end(), [](StatisticDataSetEntry v1, StatisticDataSetEntry v2){ return v1.player == v2.player ? v1.day < v2.day : v1.player < v2.player; });

	PlayerColor tmpColor = PlayerColor::NEUTRAL;
	std::vector<float> tmpColorSet;
	std::map<ColorRGBA, std::vector<float>> plotData;
	for(auto & val : tmpData)
	{
		if(tmpColor != val.player)
		{
			if(tmpColorSet.size())
				plotData[graphics->playerColors[tmpColor.getNum()]] = tmpColorSet;
			tmpColorSet.clear();
			tmpColor = val.player;
		}
		tmpColorSet.push_back(selector(val));
	}
	plotData[graphics->playerColors[tmpColor.getNum()]] = tmpColorSet;

	return plotData;
}

LineChart::LineChart(Rect position, std::string title, std::map<ColorRGBA, std::vector<float>> data) : CIntObject()
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;

	pos = position + pos.topLeft();

	auto chartArea = pos.resize(-50);
	chartArea.moveTo(Point(50, 50));

	layout.push_back(std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, title));

	canvas = std::make_shared<GraphicalPrimitiveCanvas>(Rect(0, 0, pos.w, pos.h));

	// Axis
	canvas->addLine(chartArea.topLeft() + Point(0, -10), chartArea.topLeft() + Point(0, chartArea.h + 10), Colors::WHITE);
	canvas->addLine(chartArea.topLeft() + Point(-10, chartArea.h), chartArea.topLeft() + Point(chartArea.w + 10, chartArea.h), Colors::WHITE);
	
	canvas->addLine(chartArea.topLeft(), chartArea.bottomRight(), Colors::GREEN);
}
