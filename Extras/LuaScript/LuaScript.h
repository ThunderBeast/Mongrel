//
// Copyright (c) 2008-2013 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "Context.h"
#include "Object.h"

struct lua_State;

namespace Urho3D
{

class Scene;

/// Lua script subsystem.
class LuaScript : public Object
{
    OBJECT(LuaScript);

public:
    /// Construct.
    LuaScript(Context* context);
    /// Destruct.
    ~LuaScript();

    /// Execute script file.
    bool ExecuteFile(const String& fileName);

    /// Execute script string.
    bool ExecuteString(const String& string);

    /// Execute script function.
    bool ExecuteFunction(const String& functionName);

    /// Script send event.
	void ScriptSendEvent(const String& eventName, VariantMap& eventData);

    /// Script subscribe event.
    void ScriptSubscribeToEvent(const String& eventName, const String& functionName);

private:
    /// Replace print function.
    void ReplacePrintFunction();

    /// Print function.
    static int Print(lua_State* L);

    /// Find Lua function.
    bool FindFunction(const String& functionName);

    /// Handle event.
    void HandleEvent(StringHash eventType, VariantMap& eventData);

    /// Handle a console command event.
    void HandleConsoleCommand(StringHash eventType, VariantMap& eventData);

private:
    /// Lua state.
    lua_State* luaState_;
    /// Event type to function name map.
    HashMap<StringHash, Vector<String> > eventTypeToFunctionNameMap_;
};

/// Return context.
Context* GetContext();

}
