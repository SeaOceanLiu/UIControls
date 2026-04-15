#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <memory>
#include "CheckBox.h"
#include "MainWindow.h"
#include "Bench.h"
#include "ResourceLoader.h"
#include "EditBox.h"
#include "TextArea.h"

using namespace std;

shared_ptr<CheckBox> g_checkbox1;
shared_ptr<CheckBox> g_checkbox2;
shared_ptr<CheckBox> g_checkbox3;
shared_ptr<CheckBox> g_checkbox4;
shared_ptr<CheckBox> g_checkbox5;
shared_ptr<CheckBox> g_checkbox6;
shared_ptr<CheckBox> g_checkbox7;
shared_ptr<CheckBox> g_checkbox8;
shared_ptr<CheckBox> g_checkbox9;
shared_ptr<CheckBox> g_checkbox10;
shared_ptr<CheckBox> g_checkbox11;
shared_ptr<CheckBox> g_checkbox12;
shared_ptr<CheckBox> g_checkbox13;
shared_ptr<CheckBox> g_checkbox14;
shared_ptr<CheckBox> g_checkbox15;
shared_ptr<CheckBox> g_checkbox16;

void debugTraceOutput(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    static FILE* logFile = nullptr;
    if (!logFile) {
        logFile = fopen("checkbox_log.txt", "w");
    }
    if (logFile) {
        fprintf(logFile, "Category[%d], priority[%d]: %s\n", category, priority, message);
        fflush(logFile);
    }
    cout << "Category[" << category << "], priority[" << priority << "]: " << message << endl;
}

void testBenchInitialize(void) {
    SDL_Log("testCheckBoxInitialize");
    
    StateColor redBorder(StateColor::Type::Border);
    redBorder.setNormal({255, 0, 0, 255});
    
    g_checkbox1 = make_shared<CheckBox>(nullptr, SRect(50, 50, 200, 30));
    g_checkbox1->setCaption("Accept Terms");
    g_checkbox1->setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
        cout << "Checkbox1 state changed to: " << (int)state << endl;
    });
    BENCH->addControl(g_checkbox1);
    
    g_checkbox2 = make_shared<CheckBox>(nullptr, SRect(50, 100, 200, 30));
    g_checkbox2->setCaption("Enable Feature");
    g_checkbox2->setCheckState(CheckState::Checked);
    g_checkbox2->setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
        cout << "Checkbox2 state changed to: " << (int)state << endl;
    });
    BENCH->addControl(g_checkbox2);
    
    g_checkbox3 = CheckBoxBuilder(nullptr, SRect(50, 150, 200, 30))
        .setStyle(CheckBoxStyle::Cross)
        .setCaption("Cross Style")
        .setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
            cout << "Checkbox3 (Cross) state: " << (int)state << endl;
        })
        .build();
    BENCH->addControl(g_checkbox3);
    
    g_checkbox4 = CheckBoxBuilder(nullptr, SRect(50, 200, 200, 30))
        .setStyle(CheckBoxStyle::Circle)
        .setCaption("Circle Style")
        .setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
            cout << "Checkbox4 (Circle) state: " << (int)state << endl;
        })
        .build();
    BENCH->addControl(g_checkbox4);
    
    g_checkbox5 = CheckBoxBuilder(nullptr, SRect(50, 250, 200, 30))
        .setLayout(CheckBoxLayout::TextLeft)
        .setCaption("Text on Left")
        .setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
            cout << "Checkbox5 (TextLeft) state: " << (int)state << endl;
        })
        .build();
    BENCH->addControl(g_checkbox5);
    
    g_checkbox6 = CheckBoxBuilder(nullptr, SRect(50, 300, 200, 30))
        .setCaption("Tri-state Checkbox")
        .setCheckState(CheckState::Indeterminate)
        .setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
            cout << "Checkbox6 (Tri-state) state: " << (int)state << endl;
        })
        .build();
    BENCH->addControl(g_checkbox6);
    
    g_checkbox7 = CheckBoxBuilder(nullptr, SRect(50, 350, 200, 30))
        .setCaption("Disabled Checkbox")
        .setEnable(false)
        .setCheckState(CheckState::Checked)
        .build();
    BENCH->addControl(g_checkbox7);
    
    g_checkbox8 = CheckBoxBuilder(nullptr, SRect(300, 50, 250, 30))
        .setCaption("Custom Check Color")
        .setCheckState(CheckState::Checked)
        .setCheckColor({255, 0, 255, 255})
        .build();
    BENCH->addControl(g_checkbox8);
    
    g_checkbox9 = CheckBoxBuilder(nullptr, SRect(300, 100, 250, 30))
        .setStyle(CheckBoxStyle::Cross)
        .setCaption("Custom Cross Color")
        .setCheckState(CheckState::Checked)
        .setCrossColor({0, 255, 255, 255})
        .build();
    BENCH->addControl(g_checkbox9);
    
    g_checkbox10 = CheckBoxBuilder(nullptr, SRect(300, 150, 250, 30))
        .setCaption("Custom Indeterminate Color")
        .setCheckState(CheckState::Indeterminate)
        .setIndeterminateColor({255, 165, 0, 255})
        .build();
    BENCH->addControl(g_checkbox10);
    
    g_checkbox11 = CheckBoxBuilder(nullptr, SRect(300, 200, 300, 60), 2.0f, 2.0f)
        .setCaption("2x缩放复选框")
        .setCheckState(CheckState::Checked)
        .setStyle(CheckBoxStyle::Circle)
        .setFontSize(24)
        .build();
    BENCH->addControl(g_checkbox11);
    
    g_checkbox12 = CheckBoxBuilder(nullptr, SRect(600, 50, 250, 30))
        .setCaption("Custom Box Border Color")
        .setCheckState(CheckState::Checked)
        .setBoxBorderColor({0, 128, 255, 255})
        .build();
    BENCH->addControl(g_checkbox12);
    
    g_checkbox13 = CheckBoxBuilder(nullptr, SRect(600, 100, 200, 30))
        .setCaption("FontSize 12")
        .setFontSize(12)
        .setCheckState(CheckState::Checked)
        .build();
    BENCH->addControl(g_checkbox13);
    
    g_checkbox14 = CheckBoxBuilder(nullptr, SRect(600, 140, 200, 30))
        .setCaption("FontSize 20")
        .setFontSize(20)
        .setCheckState(CheckState::Checked)
        .build();
    BENCH->addControl(g_checkbox14);
    
    g_checkbox15 = CheckBoxBuilder(nullptr, SRect(600, 190, 250, 80))
        .setCaption("Line 1\nLine 2\nLine 3")
        .setCheckState(CheckState::Checked)
        .setVerticalAlign(CheckBoxVerticalAlign::Center)
        .build();
    BENCH->addControl(g_checkbox15);
    
    g_checkbox16 = CheckBoxBuilder(nullptr, SRect(600, 280, 250, 30))
        .setCaption("Two-State Only (No Tri-state)")
        .setCheckState(CheckState::Checked)
        .setTriStateEnabled(false)
        .build();
    BENCH->addControl(g_checkbox16);
    
    SDL_Log("CheckBox test controls created");
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    BENCH->setOnInitial(testBenchInitialize);
    
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetLogOutputFunction(debugTraceOutput, nullptr);
    
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    cout << "SDL_TOUCH_MOUSE_EVENTS = " << SDL_GetHint(SDL_HINT_TOUCH_MOUSE_EVENTS) << endl;
    
    SDL_SetAppMetadata("CheckBoxTest", "1.0.0", "com.example.checkbox");
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        cout << "Failed to initialize SDL: " << SDL_GetError() << endl;
        return SDL_APP_FAILURE;
    }
    if (!TTF_Init()) {
        SDL_Log("Couldn't initialise SDL_ttf: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
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
    TTF_Quit();
}