#include "Engine.h"
#include "FileSystem.h"
#include "Log.h"
#include "Main.h"
#include "ProcessUtils.h"
#include "ResourceCache.h"
#include "ResourceEvents.h"
#include "Script.h"
#include "ScriptFile.h"
#include "QuakeToon.h"

#ifdef ENABLE_LUA
#include "LuaScript.h"
#endif

#include "DebugNew.h"

DEFINE_APPLICATION_MAIN(::Urho3D::QuakeToon);

namespace Urho3D
{

QuakeToon::QuakeToon(Context* context) :
    Application(context)
{
}

void QuakeToon::Setup()
{
    // Check for script file name
    const Vector<String>& arguments = GetArguments();
    String scriptFileName;
    for (unsigned i = 0; i < arguments.Size(); ++i)
    {
        if (arguments[i][0] != '-')
        {
            scriptFileName_ = GetInternalPath(arguments[i]);
            break;
        }
    }
    
    #if defined(ANDROID) || defined(IOS)
    // Can not pass script name on mobile devices, so choose a hardcoded default
    scriptFileName_ = "Scripts/NinjaSnowWar.as";
    #endif
    
    // Show usage if not found
    if (scriptFileName_.Empty())
    {
        ErrorExit("Usage: Urho3D <scriptfile> [options]\n\n"
            "The script file should implement the function void Start() for initializing the "
            "application and subscribing to all necessary events, such as the frame update.\n"
            #ifndef WIN32
            "\nCommand line options:\n"
            "-x<res>     Horizontal resolution\n"
            "-y<res>     Vertical resolution\n"
            "-m<level>   Enable hardware multisampling\n"
            "-v          Enable vertical sync\n"
            "-t          Enable triple buffering\n"
            "-w          Start in windowed mode\n"
            "-s          Enable resizing when in windowed mode\n"
            "-q          Enable quiet mode which does not log to standard output stream\n"
            "-b<length>  Sound buffer length in milliseconds\n"
            "-r<freq>    Sound mixing frequency in Hz\n"
            "-headless   Headless mode. No application window will be created\n"
            "-log<level> Change the log level, valid 'level' values are 'debug', 'info', 'warning', 'error'\n"
            "-prepass    Use light pre-pass rendering\n"
            "-deferred   Use deferred rendering\n"
            "-lqshadows  Use low-quality (1-sample) shadow filtering\n"
            "-noshadows  Disable shadow rendering\n"
            "-nolimit    Disable frame limiter\n"
            "-nothreads  Disable worker threads\n"
            "-nosound    Disable sound output\n"
            "-noip       Disable sound mixing interpolation\n"
            "-sm2        Force SM2.0 rendering\n"
            #endif
        );
    }
}

void QuakeToon::Start()
{
#ifdef ENABLE_LUA
    String extension = GetExtension(scriptFileName_).ToLower();
    if (extension != ".lua")
    {
#endif
        // Instantiate and register the AngelScript subsystem
        context_->RegisterSubsystem(new Script(context_));

        // Hold a shared pointer to the script file to make sure it is not unloaded during runtime
        scriptFile_ = context_->GetSubsystem<ResourceCache>()->GetResource<ScriptFile>(scriptFileName_);

        // If script loading is successful, proceed to main loop
        if (scriptFile_ && scriptFile_->Execute("void Start()"))
        {
            // Subscribe to script's reload event to allow live-reload of the application
            SubscribeToEvent(scriptFile_, E_RELOADSTARTED, HANDLER(QuakeToon, HandleScriptReloadStarted));
            SubscribeToEvent(scriptFile_, E_RELOADFINISHED, HANDLER(QuakeToon, HandleScriptReloadFinished));
            SubscribeToEvent(scriptFile_, E_RELOADFAILED, HANDLER(QuakeToon, HandleScriptReloadFailed));
            return;
        }
#ifdef ENABLE_LUA
    }
    else
    {
        // Instantiate and register the Lua script subsystem
        context_->RegisterSubsystem(new LuaScript(context_));
        LuaScript* luaScript = GetSubsystem<LuaScript>();

        // If script loading is successful, proceed to main loop
        if (luaScript->ExecuteFile(scriptFileName_.CString()))
        {
            luaScript->ExecuteFunction("Start");
            return;
        }
    }
#endif

    // The script was not successfully loaded. Show the last error message and do not run the main loop
    ErrorExit();
}

void QuakeToon::Stop()
{
    if (scriptFile_)
    {
        // Execute the optional stop function
        if (scriptFile_->GetFunction("void Stop()"))
            scriptFile_->Execute("void Stop()");
    }
#ifdef ENABLE_LUA
    else
    {
        LuaScript* luaScript = GetSubsystem<LuaScript>();
        if (luaScript)
            luaScript->ExecuteFunction("Stop");
    }
#endif
}

void QuakeToon::HandleScriptReloadStarted(StringHash eventType, VariantMap& eventData)
{
    if (scriptFile_->GetFunction("void Stop()"))
        scriptFile_->Execute("void Stop()");
}

void QuakeToon::HandleScriptReloadFinished(StringHash eventType, VariantMap& eventData)
{
    // Restart the script application after reload
    if (!scriptFile_->Execute("void Start()"))
    {
        scriptFile_.Reset();
        ErrorExit();
    }
}

void QuakeToon::HandleScriptReloadFailed(StringHash eventType, VariantMap& eventData)
{
    scriptFile_.Reset();
    ErrorExit();
}

}

