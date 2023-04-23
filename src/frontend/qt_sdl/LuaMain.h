#ifndef LUAMAIN_H
#define LUAMAIN_H
#include <vector>
#include <string.h>
#include <QDialog>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <QPushButton>
#include <lua/lua.hpp>
#include <QThread>

class LuaConsole: public QPlainTextEdit
{
    Q_OBJECT
public:
    LuaConsole(QWidget* parent=nullptr);
public slots:
    void onGetText(QString text);
    void onClear();
};


class LuaConsoleDialog: public QDialog
{
    Q_OBJECT
public:
    LuaConsoleDialog(QWidget*parent=nullptr);
    LuaConsole* console;
    QFileInfo currentScript;
    QPushButton* buttonOpenScript;
    QPushButton* buttonStartStop;
    QPushButton* buttonPausePlay;
protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void signalNewLua();
    void signalClosing();
public slots:
    void onStartStop();
    void onOpenScript();
};

class LuaThread : public QThread
{
    Q_OBJECT
    void run() override;
public:
    explicit LuaThread(QObject* parent = nullptr);
    void luaUpdate();
    void luaStop();
    void luaYeild();
    void luaTogglePause();
    void luaLayoutChange();
    int luaDialogFunction(lua_State*,int (*)(lua_State*));
    void luaDialogReturned();
    int luaDialogStart(lua_State*,int);
    void luaDialogClosed();
    void luaPrint(QString string);
    void luaClearConsole();
    void luaStateSave(QString filename);
    void luaStateLoad(QString filename);
signals:
    void signalStarted();
    void signalChangeScreenLayout();
    void signalDialogFunction();
    void signalStartDialog();
    void signalPrint(QString string);
    void signalClearConsole();
    void signalStateSave(QString filename);
    void signalStateLoad(QString filename);
private:
    bool flagRunning;
    bool flagUpdate;
    bool flagDialogReturn;
    bool flagDialogClosed;
    bool flagPaused=false;
};

extern LuaThread* luaThread;

struct OverlayCanvas
{
    QImage* imageBuffer; // buffer edited by luascript
    QImage* displayBuffer; //buffer displayed on screen
    QImage* buffer1;
    QImage* buffer2;
    QRect rectangle;
    bool isActive = true; // only active overlays are drawn
    unsigned int GLtexture; // used by GL rendering
    OverlayCanvas(int x,int y,int w, int h, bool active);
    void flip();//used to swap buffers
};


namespace LuaScript
{

typedef int(*luaFunctionPointer)(lua_State*);
struct LuaFunction
{
    luaFunctionPointer cfunction;
    const char* name;
    LuaFunction(luaFunctionPointer,const char*,std::vector<LuaFunction*>*);
};
void LuaPrint(QString string);
void Lua_Update();
extern LuaConsoleDialog* LuaDialog;
extern QWidget* panel; // to translate from global coordinates to local
extern std::vector<OverlayCanvas> LuaOverlays;
extern OverlayCanvas* CurrentCanvas;

}



#endif