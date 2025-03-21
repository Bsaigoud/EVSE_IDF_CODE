#ifndef WEBSOCKET_CONNECT_H
#define WEBSOCKET_CONNECT_H
bool WebsocketisStarted(void);

bool WebsocketConnectedisConnected(void);
void WebsocketClientClose(void);
void WebsocketClientStop(void);
void WebsocketClientStart(void);
void websocket_app_start(uint8_t network);
void websocket_app_stop(void);
void websocket_connect(void);
void websocket_disconnect(void);
void setWebsocketPingInterval(void);
void reconnect_websocket_task_delete(void);
void websocket_destroy(void);

#endif