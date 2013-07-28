#pragma once

#include "Application.h"

namespace Urho3D
{

class ScriptFile;

/// QuakeToon!
class QuakeToon : public Application
{
    OBJECT(QuakeToon);
    
public:
    /// Construct.
    QuakeToon(Context* context);
    
    /// Setup before engine initialization. Verify that a script file has been specified.
    virtual void Setup();
    /// Setup after engine initialization. Load the script and execute its start function.
    virtual void Start();
    /// Cleanup after the main loop. Run the script's stop function if it exists.
    virtual void Stop();
    
private:
    /// Handle reload start of the script file.
    void HandleScriptReloadStarted(StringHash eventType, VariantMap& eventData);
    /// Handle reload success of the script file.
    void HandleScriptReloadFinished(StringHash eventType, VariantMap& eventData);
    /// Handle reload failure of the script file.
    void HandleScriptReloadFailed(StringHash eventType, VariantMap& eventData);
    
    /// Script file name.
    String scriptFileName_;
    /// Script file.
    SharedPtr<ScriptFile> scriptFile_;
};

}