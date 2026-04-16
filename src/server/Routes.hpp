#pragma once

namespace httplib { class Server; }

namespace avacli {

struct ServerContext;

void registerAuthRoutes(httplib::Server& svr, ServerContext ctx);
void registerChatRoutes(httplib::Server& svr, ServerContext ctx);
void registerFileRoutes(httplib::Server& svr, ServerContext ctx);
void registerToolRoutes(httplib::Server& svr, ServerContext ctx);
void registerDataRoutes(httplib::Server& svr, ServerContext ctx);
void registerSettingsRoutes(httplib::Server& svr, ServerContext ctx);
void registerInfraRoutes(httplib::Server& svr, ServerContext ctx);
void registerDBRoutes(httplib::Server& svr, ServerContext ctx);
void registerKnowledgeRoutes(httplib::Server& svr, ServerContext ctx);
void registerAppRoutes(httplib::Server& svr, ServerContext ctx);
void registerAppDBRoutes(httplib::Server& svr, ServerContext ctx);
void registerServiceRoutes(httplib::Server& svr, ServerContext ctx);
void registerSystemRoutes(httplib::Server& svr, ServerContext ctx);
void registerRelayRoutes(httplib::Server& svr, ServerContext ctx);
void registerSubAgentRoutes(httplib::Server& svr, ServerContext ctx);

} // namespace avacli
