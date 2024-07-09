#ifndef LUASCRIPT_H
#define LUASCRIPT_H
#include <QDialog>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <lua.hpp>

namespace LuaScript
{

class LuaConsole : public QPlainTextEdit
{
    Q_OBJECT
public:
    LuaConsole(QWidget* parent = nullptr);
public slots:
    void onGetText(const QString& string);
    void onClear();
};

class LuaConsoleDialog : public QDialog
{
    Q_OBJECT
public:
    LuaConsoleDialog(QWidget* parent);
    LuaConsole* console;
    QFileInfo currentScript;
    QPushButton* buttonOpenScript;
    QPushButton* buttonStartStop;
    QPushButton* buttonPausePlay;
    QScrollBar* bar;

protected:
    void closeEvent(QCloseEvent* event) override;
signals:
    void signalNewLua();
    void signalClosing();
public slots:
    // void onStartStop();
    void onOpenScript();
    void onStop();
    void onPausePlay();
};

struct OverlayCanvas
{
    QImage* imageBuffer;   // buffer edited by luascript
    QImage* displayBuffer; // buffer displayed on screen
    QImage* buffer1;
    QImage* buffer2;
    QRect rectangle;

    bool isActive = true;   // only active overlays are drawn
    unsigned int GLTexture; // used by GL rendering
    bool GLTextureLoaded;
    OverlayCanvas(int x, int y, int w, int h, bool active);
    void flip();  // used to swap buffers
    bool flipped; // used to signal update to graphics.
};

void luaResetOSD();
void luaUpdate();
void luaPrint(QString string);
void luaClearConsole();
void luaHookFunction(lua_State*, lua_Debug*);
extern QWidget* panel;
extern lua_State* MainLuaState;
extern bool FlagPause;
extern bool FlagStop;
extern bool FlagNewLua;

typedef int (*luaFunctionPointer)(lua_State*);

struct LuaFunction
{
    luaFunctionPointer cfunction;
    const char* name;
    LuaFunction(luaFunctionPointer, const char*, std::vector<LuaFunction*>*);
};

extern LuaConsoleDialog *LuaDialog;
void createLuaState();

extern std::vector<OverlayCanvas> LuaOverlays;
extern OverlayCanvas* CurrentCanvas;
extern QHash<QString, QImage> ImageHash;
extern int RightPadding;
extern int BottomPadding;
extern int TopPadding;
extern int LeftPadding;

} // LuaScript
#endif
