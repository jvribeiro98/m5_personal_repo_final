#pragma once

#include <WebServer.h>

namespace Obd2Web {
void registerRoutes(WebServer& server);
String homeTile();
}
