#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QStandardItemModel>
#include "mapcontroller.h"
#include "../lib/Terrain.h"


class CMap;
class ObjectBrowser;
class CGObjectInstance;

namespace Ui
{
	class MainWindow;
	const QString teamName = "VCMI Team";
	const QString appName = "VCMI Map Editor";
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

	const QString mainWindowSizeSetting = "MainWindow/Size";
	const QString mainWindowPositionSetting = "MainWindow/Position";

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

	void initializeMap(bool isNew);

	void saveMap();
	
	MapView * mapView();

	void loadObjectsTree();

	void setStatusMessage(const QString & status);

	int getMapLevel() const {return mapLevel;}
	
	MapController controller;

private slots:
    void on_actionOpen_triggered();

	void on_actionSave_as_triggered();

	void on_actionNew_triggered();

	void on_actionLevel_triggered();

	void on_actionSave_triggered();

	void on_actionErase_triggered();
	
	void on_actionUndo_triggered();

	void on_actionRedo_triggered();

	void on_actionPass_triggered(bool checked);

	void on_actionGrid_triggered(bool checked);

	void on_toolBrush_clicked(bool checked);

	void on_toolArea_clicked(bool checked);

	void terrainButtonClicked(Terrain terrain);

	void on_toolErase_clicked();

	void on_treeView_activated(const QModelIndex &index);

	void on_terrainFilterCombo_currentTextChanged(const QString &arg1);

	void on_filter_textChanged(const QString &arg1);

	void on_actionFill_triggered();

	void on_toolBrush2_clicked(bool checked);

	void on_toolBrush4_clicked(bool checked);

	void on_inspectorWidget_itemChanged(QTableWidgetItem *item);

	void on_actionMapSettings_triggered();

	void on_actionPlayers_settings_triggered();

	void on_actionValidate_triggered();

	void on_actionUpdate_appearance_triggered();

	void on_actionRecreate_obstacles_triggered();

public slots:

	void treeViewSelected(const QModelIndex &selected, const QModelIndex &deselected);
	void loadInspector(CGObjectInstance * obj, bool switchTab);
	void mapChanged();
	void enableUndo(bool enable);
	void enableRedo(bool enable);
	void onSelectionMade(int level, bool anythingSelected);

private:
	void preparePreview(const QModelIndex &index, bool createNew);
	void addGroupIntoCatalog(const std::string & groupName, bool staticOnly);
	void addGroupIntoCatalog(const std::string & groupName, bool staticOnly, int ID);

	void changeBrushState(int idx);
	void setTitle();

	void loadUserSettings();
	void saveUserSettings();

private:
    Ui::MainWindow *ui;
	ObjectBrowser * objectBrowser = nullptr;
	QGraphicsScene * scenePreview;
	
	QString filename;
	bool unsaved = false;

	QStandardItemModel objectsModel;

	int mapLevel = 0;

	std::set<int> catalog;
};

#endif // MAINWINDOW_H
