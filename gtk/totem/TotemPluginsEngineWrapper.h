/*
 * SPDX-License-Identifier: GPL-3-or-later
 */

 

#include <glib-object.h>
#include <glibmm/refptr.h>
#include <gtkmm/builder.h>
#include <glibmm.h>

#include <memory>

extern "C" {
#include "totem-plugins-engine.h"
}


class TotemPluginsEngineWrapper {
public:
    // Constructor: initializes the engine by calling the C function
    explicit TotemPluginsEngineWrapper(TotemObject* totem)
        : engine_(totem_plugins_engine_get_default(totem)) {
        // Ensure the engine is valid
        if (engine_ == nullptr) {
            throw std::runtime_error("Failed to initialize TotemPluginsEngine.");
        }
    }

    // Destructor: cleans up the engine by calling the C function
    ~TotemPluginsEngineWrapper() {
        if (engine_ != nullptr) {
            totem_plugins_engine_shut_down(engine_);
            g_clear_object(reinterpret_cast<GObject**>(&engine_));
        }
    }

    // Disable copy and move semantics
    TotemPluginsEngineWrapper(const TotemPluginsEngineWrapper&) = delete;
    TotemPluginsEngineWrapper& operator=(const TotemPluginsEngineWrapper&) = delete;
    TotemPluginsEngineWrapper(TotemPluginsEngineWrapper&&) = delete;
    TotemPluginsEngineWrapper& operator=(TotemPluginsEngineWrapper&&) = delete;

    // Provide access to the underlying TotemPluginsEngine C object
    TotemPluginsEngine* get() const {
        return engine_;
    }

private:
    TotemPluginsEngine* engine_;
};



