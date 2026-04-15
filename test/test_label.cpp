#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <memory>
#include "Label.h"
#include "MainWindow.h"
#include "Bench.h"
#include "EventQueue.h"
#include "ResourceLoader.h"
#include "EditBox.h"
#include "TextArea.h"

using namespace std;

shared_ptr<Label> g_label1;
shared_ptr<Label> g_label2;
shared_ptr<Label> g_label3;
shared_ptr<Label> g_label4;
shared_ptr<Label> g_label5;
shared_ptr<Label> g_label6;
shared_ptr<Label> g_label7;
shared_ptr<Label> g_label8;

void testBenchInitialize(void) {
    SDL_Log("testLabelInitialize");

    StateColor redBorder(StateColor::Type::Border);
    redBorder.setNormal({255, 0, 0, 255});

    shared_ptr<Label> g_label18 = LabelBuilder(nullptr, SRect(600, 960, 150, 60), 2.0f, 2.0f)
        .setCaption("2x BottomCenter")
        .setAlignmentMode(AlignmentMode::AM_BOTTOM_CENTER)
        .setBorderStateColor(redBorder)
        .build();
    BENCH->addControl(g_label18);

    SDL_Log("Label test controls created");
}

void debugTraceOutput(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    cout << "Category[" << category << "], priority[" << priority << "]: " << message << endl;
    static FILE* logFile = nullptr;
    if (!logFile) {
        logFile = fopen("label_log.txt", "w");
    }
    if (logFile) {
        fprintf(logFile, "Category[%d], priority[%d]: %s\n", category, priority, message);
        fflush(logFile);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    cout << "Using SDL3 library for Label test" << endl;
    
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetLogOutputFunction(debugTraceOutput, nullptr);
    
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    cout << "SDL_TOUCH_MOUSE_EVENTS = " << SDL_GetHint(SDL_HINT_TOUCH_MOUSE_EVENTS) << endl;
    
    SDL_SetAppMetadata("LabelTest", "1.0.0", "com.example.labeltest");
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        cout << "Failed to initialize SDL: " << SDL_GetError() << endl;
        return SDL_APP_FAILURE;
    }
    if (!TTF_Init()) {
        SDL_Log("Couldn't initialise SDL_ttf: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    BENCH->setOnInitial(testBenchInitialize);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    shared_ptr<Event> gameEvent = nullptr;
    
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
            
        case SDL_EVENT_WINDOW_RESIZED:
            MAINWIN->handleWindowEvent(event->window);
            BENCH->resized({0, 0, (float)event->window.data1, (float)event->window.data2});
            break;
            
        case SDL_EVENT_WINDOW_MOVED:
            MAINWIN->handleWindowEvent(event->window);
            break;
            
        case SDL_EVENT_KEY_DOWN:
            {
                KeyEventData keyData;
                keyData.keycode = event->key.key;
                keyData.scancode = event->key.scancode;
                keyData.mod = event->key.mod;
                keyData.repeat = event->key.repeat != 0;
                gameEvent = make_shared<Event>(EventName::KEY_DOWN, keyData);
                BENCH->inputControl(gameEvent);
            }
            break;
            
        case SDL_EVENT_TEXT_INPUT:
            {
                TextInputEventData textData;
                textData.text = event->text.text;
                textData.start = -1;
                textData.length = -1;
                gameEvent = make_shared<Event>(EventName::TEXT_INPUT, textData);
                BENCH->inputControl(gameEvent);
            }
            break;
            
        case SDL_EVENT_TEXT_EDITING:
            break;
            
        case SDL_EVENT_MOUSE_WHEEL:
            {
                MouseWheelEventData wheelData;
                wheelData.x = event->wheel.x;
                wheelData.y = event->wheel.y;
                wheelData.mouseX = event->wheel.mouse_x;
                wheelData.mouseY = event->wheel.mouse_y;
                gameEvent = make_shared<Event>(EventName::MOUSE_WHEEL, wheelData);
                BENCH->inputControl(gameEvent);
            }
            break;
            
        case SDL_EVENT_MOUSE_MOTION:
            gameEvent = make_shared<Event>(EventName::MOUSE_MOVING, make_shared<SPoint>((float)event->motion.x, (float)event->motion.y));
            BENCH->inputControl(gameEvent);
            break;
            
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                gameEvent = make_shared<Event>(EventName::MOUSE_LBUTTON_DOWN, make_shared<SPoint>((float)event->button.x, (float)event->button.y));
                BENCH->inputControl(gameEvent);
            }
            break;
            
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                gameEvent = make_shared<Event>(EventName::MOUSE_LBUTTON_UP, make_shared<SPoint>((float)event->button.x, (float)event->button.y));
                BENCH->inputControl(gameEvent);
            }
            break;
    }
    
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    BENCH->eventLoopEntry();
    BENCH->update();
    
    SDL_SetRenderDrawColor(MainWindow::getInstance()->getRenderer(), 40, 40, 40, 255);
    SDL_RenderClear(MainWindow::getInstance()->getRenderer());
    
    BENCH->draw();

    SDL_RenderPresent(GET_RENDERER);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_Log("Application quit");
    ResourceLoader::getInstance()->detachLoadingThread();
    TTF_Quit();
}