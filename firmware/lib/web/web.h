#pragma once
// HTTP server: web app at "/", REST API under /api/*, firmware upload at /update.
// Shortcut-friendly endpoints (plain GET works):
//   /api/open  /api/close  /api/stop  /api/toggle  /api/position?value=40
//   optional: &duration=<s> or &gentle=1 for a slow move, &token=<apiToken>
//   legacy aliases: /open /close /stop /toggle
namespace web {

void begin();
void stop();
void loop();   // call every few ms from the network task
bool running();

}  // namespace web
