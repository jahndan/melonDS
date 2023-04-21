#include "LuaMain.h"
#include <QScrollBar>
#include <QFileDialog>
#include <string.h>
#include <filesystem>
#include "types.h"
#include "NDS.h"
#include <QGuiApplication>

#include <QPainter>

using namespace LuaScript;

LuaConsoleDialog* LuaScript::LuaDialog=nullptr;
LuaThread::LuaThread(QObject* parent)
{
    this->setParent(parent);
}

LuaConsoleDialog::LuaConsoleDialog(QWidget* parent)
{
    this->setParent(parent);
    console = new LuaConsole(this);
    console->setGeometry(0,20,302,80);
    buttonPausePlay = new QPushButton("Pause/UnPause",this);
    buttonPausePlay->setGeometry(0,0,100,20);
    buttonStartStop = new QPushButton("Stop",this);
    buttonStartStop->setGeometry(101,0,100,20);
    buttonOpenScript = new QPushButton("OpenLuaFile",this);
    buttonOpenScript->setGeometry(202,0,100,20);
    connect(buttonOpenScript,&QPushButton::clicked,this,&LuaConsoleDialog::onOpenScript);
    this->setWindowTitle("Lua Script");
}

void LuaConsoleDialog::onOpenScript()
{
    QFileInfo file = QFileDialog::getOpenFileName(this, "Load Lua Script",QDir::currentPath());
    if (!file.exists())
        return;
    currentScript=file;
    if (luaThread != nullptr)
    {
        luaThread->terminate();//might cause issues!
        luaThread->wait();
        luaThread->deleteLater();
        LuaScript::LuaOverlays.clear();
    }
    luaThread = new LuaThread();
    connect(luaThread,&LuaThread::signalPrint,console,&LuaConsole::onGetText);
    connect(luaThread,&LuaThread::signalClearConsole,console,&LuaConsole::onClear);
    connect(buttonPausePlay,&QPushButton::clicked,luaThread,&LuaThread::luaTogglePause);
    connect(buttonStartStop,&QPushButton::clicked,luaThread,&LuaThread::luaStop);
    connect(buttonStartStop,&QPushButton::clicked,this,&LuaConsoleDialog::onStartStop);
    connect(this,&LuaConsoleDialog::signalClosing,luaThread,&LuaThread::luaStop);
    emit signalNewLua();
}

void LuaConsoleDialog::closeEvent(QCloseEvent *event)
{
    if (luaThread==nullptr or !(luaThread->isRunning()))
    {
        event->accept();
        return;
    }
    emit signalClosing();
    if (!luaThread->wait(2000))//2 seconds
    {
        printf("Force Stoping...");
        luaThread->terminate();//if lua thread is stuck terminate the thread
    }
    luaThread->wait();
    printf("Closed");
    event->accept();

}

LuaConsole::LuaConsole(QWidget* parent)
{
    this->setParent(parent);
}

void LuaConsole::onGetText(QString string)
{
    this->appendPlainText(string);
    QScrollBar* bar = verticalScrollBar();
    bar->setValue(bar->maximum());
}
void LuaConsole::onClear()
{
    this->clear();
}

void LuaThread::luaClearConsole()
{
    emit signalClearConsole();
}

void LuaScript::LuaPrint(QString string)
{
    LuaDialog->console->onGetText(string);
}

//Used by "while true framestep" loops
void LuaThread::luaYeild()
{
    LuaThread::exec();   
}

void LuaThread::luaPrint(QString string)
{
    emit signalPrint(string);
}

void LuaThread::luaStateSave(QString filename)
{
    emit signalStateSave(filename);
}

void LuaThread::luaStateLoad(QString filename)
{
    emit signalStateLoad(filename);
}

std::vector<LuaFunction*> LuaFunctionList; // List of all lua functions.
lua_State* MainLuaState;//Currently only support one lua script at a time
bool frameFlag = false;
QWidget* LuaScript::panel = nullptr;

LuaFunction::LuaFunction(luaFunctionPointer cfunctionPointer,const char* LuaFunctionName,std::vector<LuaFunction*>* container)
{
    this->cfunction=cfunctionPointer;
    this->name=LuaFunctionName;
    container->push_back(this);
}

void LuaConsoleDialog::onStartStop()
{
    if(luaThread==nullptr or !(luaThread->isRunning()))
        return;
    if (!luaThread->wait(5000))//5 seconds
    {
        luaThread->terminate();//if lua thread is stuck terminate the thread
        console->onGetText(QString("Force Stopped"));
    }
    luaThread->wait();
}

void LuaThread::luaStop()
{
    flagRunning=false;
    if (isRunning())
        exit();
}


void LuaThread::luaTogglePause()
{
    flagPaused = !flagPaused;
    exit();   
}

void LuaThread::run()
{
    MainLuaState=nullptr;
    std::string fileName = LuaDialog->currentScript.fileName().toStdString();
    std::string filedir = LuaDialog->currentScript.dir().path().toStdString();
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    for(LuaFunction* function : LuaFunctionList)
        lua_register(L,function->name,function->cfunction);
    std::filesystem::current_path(filedir.c_str());
    if (luaL_dofile(L,&fileName[0])==LUA_OK)
    {
        MainLuaState=L;
        flagRunning=true;
    }
    else
    {
        LuaPrint(lua_tostring(L,-1));
        printf("Error: %s\n", lua_tostring(L, -1));
        MainLuaState=nullptr;
        flagRunning=false;
    }  
    //Lua Main loop
    flagPaused = false;
    while (flagRunning)
    {
        exec();
    }
    flagPaused = true;
    emit signalPrint("Stopped");
}

//Is called every frame advance
void LuaThread::luaUpdate()
{
    if(!isRunning())
        return;
    if(!flagPaused)
    {
        frameFlag = true; // used for yeild
        LuaScript::Lua_Update();
    }
        
    exit();
}

//_Update is called once every "emulator" frame (so every frame advance)
void LuaScript::Lua_Update()
{
    if (MainLuaState==nullptr)
        return;
    if (lua_getglobal(MainLuaState,"_Update")!=LUA_TFUNCTION)
        return;
    if(lua_pcall(MainLuaState,0,0,0)!=0)
    {
        luaThread->luaPrint(lua_tostring(MainLuaState,-1));
        printf( "Error: %s\n", lua_tostring(MainLuaState, -1));
    }
}








/*
*   Front End Stuff
*/

OverlayCanvas::OverlayCanvas(int x,int y,int width,int height,bool isActive)
{
    this->isActive=isActive;
    buffer1 = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    buffer2 = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    buffer1->fill(0xffffff00); //initializes buffer with yellow pixels
    buffer2->fill(0xffffff00);
    imageBuffer = buffer1;
    displayBuffer = buffer2;
    rectangle = QRect(x,y,width,height);
}
void OverlayCanvas::flip()
{
    if (imageBuffer == buffer1)
    {
        imageBuffer = buffer2;
        displayBuffer = buffer1;
    }
    else
    {
        imageBuffer = buffer1;
        displayBuffer = buffer2;
    }
}

std::vector<OverlayCanvas> LuaScript::LuaOverlays;
OverlayCanvas* LuaScript::CurrentCanvas;







//Add lua function to the list of defined functions.
#define AddLuaFunction(functPointer,name)LuaScript::LuaFunction name(functPointer,#name,&LuaFunctionList)

/*
*   Start Defining Lua Functions:
*/

namespace LuaFunctionDefinition
{
//TODO: make lua function names consistent
//TODO: organize luafunctions into seperate tabels


int lua_MelonPrint(lua_State* L)
{
    QString string = luaL_checkstring(L,1);
    luaThread->luaPrint(string);
    return 0;
}
AddLuaFunction(lua_MelonPrint,MelonPrint);

int lua_MelonClear(lua_State* L)
{
    luaThread->luaClearConsole();
    return 0;
}
AddLuaFunction(lua_MelonClear,MelonClear);

enum ramInfo_ByteType
{
    ramInfo_OneByte = 1, 
    ramInfo_TwoBytes = 2, 
    ramInfo_FourBytes = 4
};

u32 GetMainRAMValueU(const u32& addr, const ramInfo_ByteType& byteType)
{
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(u8*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_TwoBytes:
        return *(u16*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_FourBytes:
        return *(u32*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    default:
        return 0;
    }
}

s32 GetMainRAMValueS(const u32& addr, const ramInfo_ByteType& byteType)
{
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(s8*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_TwoBytes:
        return *(s16*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_FourBytes:
        return *(s32*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    default:
        return 0;
    }
}


int Lua_ReadDatau(lua_State* L,ramInfo_ByteType byteType) 
{   
    u32 address = luaL_checkinteger(L,1);
    u32 value =GetMainRAMValueU(address,byteType);
    lua_pushinteger(L, value);
    return 1;
}

int Lua_ReadDatas(lua_State* L,ramInfo_ByteType byteType)
{
    u32 address = luaL_checkinteger(L,1);
    s32 value = GetMainRAMValueS(address,byteType);
    lua_pushinteger(L, value);
    return 1;
}

int Lua_Readu8(lua_State* L)
{
    return Lua_ReadDatau(L,ramInfo_OneByte);
}
AddLuaFunction(Lua_Readu8,Readu8);

int Lua_Readu16(lua_State* L)
{
    return Lua_ReadDatau(L,ramInfo_TwoBytes);
}
AddLuaFunction(Lua_Readu16,Readu16);

int Lua_Readu32(lua_State* L)
{
    return Lua_ReadDatau(L,ramInfo_FourBytes);
}
AddLuaFunction(Lua_Readu32,Readu32);

int Lua_Reads8(lua_State* L)
{
    return Lua_ReadDatas(L,ramInfo_OneByte);
}
AddLuaFunction(Lua_Reads8,Reads8);

int Lua_Reads16(lua_State* L)
{
    return Lua_ReadDatas(L,ramInfo_TwoBytes);
}
AddLuaFunction(Lua_Reads16,Reads16);

int Lua_Reads32(lua_State* L)
{
    return Lua_ReadDatas(L,ramInfo_FourBytes);
}
AddLuaFunction(Lua_Reads32,Reads32);


int Lua_NDSTapDown(lua_State* L)
{
    int x =luaL_checkinteger(L,1);
    int y =luaL_checkinteger(L,2);
    NDS::TouchScreen(x,y);
    
    return 0;
}
AddLuaFunction(Lua_NDSTapDown,NDSTapDown);

int Lua_NDSTapUp(lua_State* L)
{
    NDS::ReleaseScreen();
    return 0;
}
AddLuaFunction(Lua_NDSTapUp,NDSTapUp);

int Lua_stateSave(lua_State* L)
{
    QString filename = luaL_checkstring(L,1);
    luaThread->luaStateSave(filename);
    return 0;
}
AddLuaFunction(Lua_stateSave,StateSave);

int Lua_stateLoad(lua_State* L)
{
    QString filename = luaL_checkstring(L,1);
    luaThread->luaStateLoad(filename);
    return 0;
}
AddLuaFunction(Lua_stateLoad,StateLoad);


int Lua_nextFrame(lua_State* L)
{
    frameFlag=false;
    while(!frameFlag)
    {
        luaThread->luaYeild();
    }
    return 0;
}
AddLuaFunction(Lua_nextFrame,nextFrame);

int Lua_getMouse(lua_State* L)
{
    Qt::MouseButtons btns = QGuiApplication::mouseButtons();
    QPoint pos = LuaScript::panel->mapFromGlobal(QCursor::pos(QGuiApplication::primaryScreen()));
    const char* keys[6]={"Left","Middle","Right","XButton1","XButton2","Wheel"};
    bool vals[6]=
    {
        btns.testFlag(Qt::LeftButton),
        btns.testFlag(Qt::MiddleButton),
        btns.testFlag(Qt::RightButton),
        btns.testFlag(Qt::XButton1),
        btns.testFlag(Qt::XButton2),
        false //TODO: add mouse wheel support
    };
    lua_createtable(L, 0, 8);
    lua_pushinteger(L, pos.x());
    lua_setfield(L, -2, "X");
    lua_pushinteger(L, pos.y());
    lua_setfield(L, -2, "Y");
    for(int i=0;i<6;i++)
    {
        lua_pushboolean(L,vals[i]);
        lua_setfield(L,-2,keys[i]);
    }
    return 1;//returns table describing the current pos and state of the mouse
}
AddLuaFunction(Lua_getMouse,getMouse);

/*
 *  Front end lua functions
*/
//TODO: Lua Colors 

int Lua_MakeCanvas(lua_State* L) //MakeCanvas(int x,int y,int width,int height,[bool active = true])
{
    int offset = 0;
    bool a = true;
    if (lua_isboolean(L,-1))
    {
        offset=-1;
        bool a = lua_toboolean(L,-1);
    }
    int x=lua_tonumber(L,-4+offset);
    int y=lua_tonumber(L,-3+offset);
    int w=lua_tonumber(L,-2+offset);
    int h=lua_tonumber(L,-1+offset);
    OverlayCanvas canvas(x,y,w,h,a);
    lua_pushinteger(L,LuaOverlays.size());
    LuaOverlays.push_back(canvas);
    return 1; //returns index of the new overlay
}
AddLuaFunction(Lua_MakeCanvas,MakeCanvas);

int Lua_SetCanvas(lua_State* L)
{
    int index=lua_tonumber(L,-1);
    CurrentCanvas = &LuaOverlays.at(index);
    return 0;
}
AddLuaFunction(Lua_SetCanvas,SetCanvas);
int Lua_ClearOverlay(lua_State* L)
{
    CurrentCanvas->imageBuffer->fill(0x00000000);
    return 0;
}
AddLuaFunction(Lua_ClearOverlay,clearOverlay);

int Lua_Flip(lua_State* L)
{
    CurrentCanvas->flip();
    return 0;
}
AddLuaFunction(Lua_Flip,Flip);

//text(int x, int y, string message, [u32 color = 'black'], [int fontsize = 9], [string fontfamily = Franklin Gothic Medium])
int Lua_text(lua_State* L) 
{
    int x = lua_tonumber(L,-6);
    int y = lua_tonumber(L,-5);
    const char* message = lua_tostring(L,-4);
    u32 color = lua_tonumber(L,-3);
    QString FontFamily= lua_tostring(L,-2);
    int size = lua_tonumber(L,-1);
    QPainter painter(CurrentCanvas->imageBuffer);
    QFont font(FontFamily,size,0,false);
    font.setStyleStrategy(QFont::NoAntialias);
    font.setLetterSpacing(QFont::AbsoluteSpacing,-1);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(x,y,message);
    return 0;
}
AddLuaFunction(Lua_text,text);

int Lua_line(lua_State* L)
{
    int x1 = luaL_checknumber(L,1);
    int y1 = luaL_checknumber(L,2);
    int x2 = luaL_checknumber(L,3);
    int y2 = luaL_checknumber(L,4);
    u32 color = luaL_checknumber(L,5);
    QPainter painter(CurrentCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawLine(x1,y1,x2,y2);
    return 0;
}
AddLuaFunction(Lua_line,line);

int Lua_rect(lua_State* L)
{
    u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    QPainter painter(CurrentCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawRect(x,y,width,height);
    return 0;
}
AddLuaFunction(Lua_rect,Rect);

int Lua_fillrect(lua_State* L)
{
    u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    QPainter painter(CurrentCanvas->imageBuffer);
    painter.setPen(color);
    painter.fillRect(x,y,width,height,color);
    return 0;
}
AddLuaFunction(Lua_fillrect,fillRect);


}

