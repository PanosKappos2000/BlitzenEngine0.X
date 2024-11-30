#include "blitEvents.h"
#include "mainEngine.h"

namespace BlitzenCore
{
    #define BLIT_ENGINE_SHUTDOWN_EXPECTED_EVENTS 1
    #define BLIT_KEY_PRESSED_EXPECTED_EVENTS 100
    #define BLIT_KEY_RELEASED_EXPECTED_EVENTS 100
    #define BLIT_MOUSE_BUTTON_PRESSED_EXPECTED_EVENTS 50
    #define BLIT_MOUSE_BUTTON_RELEASED_EXPECTED_EVENTS 50
    #define BLIT_MOUSE_MOVED_EXPECTED_EVENTS  10
    #define BLIT_MOUSE_WHEEL_EXPECTED_EVENTS  10
    #define BLIT_WINDOW_RESIZE_EXPECTED_EVENTS  10

    static uint32_t maxExpectedEvents[static_cast<size_t>(BlitEventType::MaxTypes)] = 
    {
        BLIT_ENGINE_SHUTDOWN_EXPECTED_EVENTS, 
        BLIT_KEY_PRESSED_EXPECTED_EVENTS, 
        BLIT_KEY_RELEASED_EXPECTED_EVENTS, 
        BLIT_MOUSE_BUTTON_PRESSED_EXPECTED_EVENTS,
        BLIT_MOUSE_BUTTON_RELEASED_EXPECTED_EVENTS,
        BLIT_MOUSE_MOVED_EXPECTED_EVENTS,
        BLIT_MOUSE_WHEEL_EXPECTED_EVENTS, 
        BLIT_WINDOW_RESIZE_EXPECTED_EVENTS
    };

    // The static variable cannot own the state as it has dynmically allocated memory, it will be a pointer to it instead
    static EventSystemState* pEventSystemState;

    uint8_t EventsInit()
    {
        static BlitzenEngine::Engine* pEngine = BlitzenEngine::Engine::GetEngineInstancePointer();
        if(!pEngine)
        {
            BLIT_ERROR("The event system cannot be initialized before the engine")
            return 0;
        }

        pEventSystemState = &(BlitzenEngine::Engine::GetEngineInstancePointer()->GetEngineSystems().eventSystemState);
        BlitzenCore::BlitMemoryZero(pEventSystemState, sizeof(EventSystemState));
        return 1;
    }

    void EventsShutdown()
    {
        if(BlitzenEngine::Engine::GetEngineInstancePointer()->GetEngineSystems().eventSystem)
        {
            BLIT_ERROR("The event system cannot be shutdow without the Engine's permission")
            return;
        }

        pEventSystemState = nullptr;
    }

    uint8_t RegisterEvent(BlitEventType type, void* pListener, pfnOnEvent eventCallback)
    {
        BlitCL::DynamicArray<RegisteredEvent>& events = pEventSystemState->registeredEvents[static_cast<size_t>(type)];
        if(!events.GetSize())
        {
            events.Reserve(maxExpectedEvents[static_cast<size_t>(type)]);
        }

        for(size_t i = 0; i < events.GetSize(); ++i)
        {
            if(events[i].pListener == pListener)
            {
                BLIT_ERROR("The same listener cannot have different registered callbacks for the same type of event")
                return 0;
            }
        }

        RegisteredEvent event;
        event.eventCallback = eventCallback;
        event.pListener = pListener;
        events.PushBack(event);
        return 1;
    }

    uint8_t UnregisterEvent(BlitEventType type, void* pListener, pfnOnEvent eventCallback)
    {
        BlitCL::DynamicArray<RegisteredEvent>& events = pEventSystemState->registeredEvents[static_cast<size_t>(type)];
        if(!events.GetSize())
        {
            return 0;
        }

        for(size_t i = 0; i < events.GetSize(); ++i)
        {
            if(events[i].pListener == pListener)
            {
                events.RemoveAtIndex(i);
                return 1;
            }
        }

        BLIT_ERROR("Event not found")
        return 0;
    }

    uint8_t FireEvent(BlitEventType type, void* pSender, EventContext eventData)
    {
        BlitCL::DynamicArray<RegisteredEvent>& events = pEventSystemState->registeredEvents[static_cast<size_t>(type)];
        if(!events.GetSize())
        {
            return 0;
        }

        for(size_t i = 0; i < events.GetSize(); ++i)
        {
            void* pListener = events[i].pListener;
            if(events[i].eventCallback(type, pSender, pListener, eventData))
            {
                // This should only happen if the listener believes the entire event is handled with their function call
                return 1;
            }   
        }

        // Not necessarily an error
        return 0;
    }




    struct KeyboardState 
    {
        uint8_t keys[256];
    };

    struct MouseState 
    {
        int16_t x;
        int16_t y;
        uint8_t buttons[static_cast<size_t>(MouseButton::MaxButtons)];
    };

    struct InputState 
    {
        KeyboardState currentKeyboard;
        KeyboardState previousKeyboard;
        MouseState currentMouse;
        MouseState previousMouse;
    };

    static InputState inputState = {};

    void InputInit() 
    {
        BLIT_INFO("Input system initialized.")
    }

    void InputShutdown() 
    {
        if(BlitzenEngine::Engine::GetEngineInstancePointer()->GetEngineSystems().inputSystem)
        {
            BLIT_ERROR("Blitzen has not given permission for input to shutdown")
            return;
        }
        // TODO: Add shutdown routines when needed.
    }

    void UpdateInput(double delta_time) 
    {
        // Copy current states to previous states
        BlitzenCore::BlitMemoryCopy(&inputState.previousKeyboard, &inputState.currentKeyboard, sizeof(KeyboardState));
        BlitzenCore::BlitMemoryCopy(&inputState.previousMouse, &inputState.currentMouse, sizeof(MouseState));
    }

    void InputProcessKey(BlitKey key, uint8_t bPressed) 
    {
        // Check If the key has not already been flagged as the value of bPressed
        if (inputState.currentKeyboard.keys[static_cast<size_t>(key)] != bPressed) 
        {
            // Change the state to bPressed
            inputState.currentKeyboard.keys[static_cast<size_t>(key)] = bPressed;

            // Fire off an event for immediate processing after saving the data of the input to the event context
            EventContext context;
            context.data.ui16[0] = static_cast<uint16_t>(key);
            FireEvent(bPressed ? BlitEventType::KeyPressed : BlitEventType::KeyReleased, nullptr, context);
        }
    }

    void InputProcessButton(MouseButton button, uint8_t bPressed) 
    {
        // If the state changed, fire an event.
        if (inputState.currentMouse.buttons[static_cast<size_t>(button)] != bPressed) 
        {
            inputState.currentMouse.buttons[static_cast<size_t>(button)] = bPressed;
            // Fire the event.
            EventContext context;
            context.data.ui16[0] = static_cast<uint16_t>(button);
            FireEvent(bPressed ? BlitEventType::MouseButtonPressed : BlitEventType::MouseButtonPressed, nullptr, context);
        }
    }

    void InputProcessMouseMove(int16_t x, int16_t y) 
    {
        // Only process if actually different
        if (inputState.currentMouse.x != x || inputState.currentMouse.y != y) {
            
            inputState.currentMouse.x = x;
            inputState.currentMouse.y = y;

            // Fire the event
            EventContext context;
            context.data.ui16[0] = x;
            context.data.ui16[1] = y;
            FireEvent(BlitEventType::MouseMoved, nullptr, context);
        }
    }
    
    void InputProcessMouseWheel(int8_t zDelta) 
    {
        // No internal state to update, simply fires an event
        EventContext context;
        context.data.ui8[0] = zDelta;
        FireEvent(BlitEventType::MouseWheel, nullptr, context);
    }

    uint8_t GetCurrentKeyState(BlitKey key) 
    {
        return inputState.currentKeyboard.keys[static_cast<size_t>(key)];
    }

    uint8_t GetPreviousKeyState(BlitKey key) 
    {
        return inputState.currentKeyboard.keys[static_cast<size_t>(key)];
    }

    
    uint8_t GetCurrentMouseButtonState(MouseButton button) 
    {
        return inputState.currentMouse.buttons[static_cast<size_t>(button)];
    }


    uint8_t GetPreviousMouseButtonState(MouseButton button)
    {
        return inputState.previousMouse.buttons[static_cast<size_t>(button)];
    }

    void GetMousePosition(int32_t* x, int32_t* y) 
    {
        *x = inputState.previousMouse.x;
        *y = inputState.currentMouse.y;
    }
    void GetPreviousMousePosition(int32_t* x, int32_t* y)
    {
        *x = inputState.previousMouse.x;
        *y = inputState.previousMouse.y;
    }
}