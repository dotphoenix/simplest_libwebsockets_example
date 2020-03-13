/*
 * simplest_websocket_example - 最简单且完整的使用libwebsocket的例子
 *
 * Copyright (C) 2020 Zhu Nengjie <dotphoenix@qq.com>
 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation:
 *  version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
*/
 
#include <libwebsockets.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
 
//这个例子的主要功能是:
//1. ws客户端会定期向ws服务器更新一个数字,该数字递增,见:LWS_CALLBACK_CLIENT_WRITEABLE;
//2. ws服务器收到该数字之后,见:LWS_CALLBACK_RECEIVE,会保存下来;
//3. 当server可写的时候,会向所有的客户端发送这个数字(包括发送数字的客户端),见:LWS_CALLBACK_SERVER_WRITEABLE
//4. 客户端收到数字的时候,会显示出来,见:LWS_CALLBACK_CLIENT_RECEIVE
//5. 浏览器访问http://localhost:8000, ws服务器(因为它同时也是一个标准的http服务器)会返回一个自动创建的一个html文件,
//   该html文件的内容就是html_contenti变量里面的内容,它的主要功能是通过js建立一个ws客户端,并连接到localhost:8000这个
//   ws服务器,并且在收到数据的时候,更新到页面.当有人向localhost:8000请求发送http请求时,返回该文件,见:LWS_CALLBACK_RECEIVE
 
 
//编译: 必须先安装libwebsocket和glib-2.0, ssl应该不是必须的,但没有的话可能编译不通过,注意修改下ssl的路径为你自己电脑的路径
//gcc -g -Wall simplest_websocket_example.c -o simplest_websocket_example `pkg-config --libs --cflags glib-2.0 libwebsockets`  -I/usr/local/Cellar/openssl@1.1/1.1.1d/include
 
//这个内容会自动写到一个html文件里面, 他的功能是使用js创建一个websocket客户端,当收到数据的时候,刷新页面
const char *html_content = " <!DOCTYPE html>"
"<html> \n"
"<title>Websocket example</title>\n"
"<script>\n"
 " var socket = new WebSocket( \"ws://\" + document.domain + ':' + location.port, \"ws-protocol-example\" );\n"
"  function update(msg) { document.getElementById(\"num\").innerHTML = msg; }\n"
 " socket.onopen = function() { console.log(\"socket open\"); update(\"open\"); }\n"
 " socket.onclose = function() { console.log(\"socket close\"); update(\"closed\"); }\n"
"  socket.onmessage = function(msg) { console.log(\"socket message: \" + msg.data); update(msg.data); }\n"
"</script>\n"
"<body>\n"
"<p id=\"num\"></p>\n"
"</body>\n"
"</html>\n";
 
//创建html文件的路径
static char example_html_file_path[512] = {0};
 
//返回html文件的路径,会自动获取当前运行的路径
static const char* get_html_file_path()
{
    if(strlen(example_html_file_path) == 0)
    {
        getcwd(example_html_file_path, 512);
        strcat(example_html_file_path, "/websocket_example.html");
    }
    return (const char*)example_html_file_path;
}
 
//写入内容到html文件里面,此处每次打开页面都会重新创建一次,可以考虑做个判断,不用每次都创建
static void create_html_file()
{
    FILE *f;
    const char* pathname = get_html_file_path();
    if ((f = fopen(pathname, "w+")) == NULL)
    {
        printf("open file error");
        return ;
    }
    fwrite(html_content, 1, strlen(html_content), f);
    fflush(f);
    fclose(f);
}
 
//html协议的回调函数
static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
 
    switch( reason )
    {
        case LWS_CALLBACK_HTTP:
            create_html_file();
            printf("%s->%s\n", "LWS_CALLBACK_HTTP", get_html_file_path());
            lws_serve_http_file( wsi, get_html_file_path(), "text/html", NULL, 0 );
            break;
        default:
            break;
    }
 
    return 0;
}
 
 
#define EXAMPLE_RX_BUFFER_BYTES (10)
//定义的一个payload结构,用于存储ws发送和接收的数据
struct payload
{
    unsigned char data[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
    size_t len;
} ;
 
struct payload server_received_payload;
 
 
//ws服务端的ws(ws-protocol-example)协议的回调函数
static int callback_example_server( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
    switch( reason )
    {
        case LWS_CALLBACK_RECEIVE: //收到数据
            memset(&server_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], 0, len + 1);
            memcpy( &server_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], in, len );
            server_received_payload.len = len;
            printf("\033[31mserver callback: %s->%s\n", "LWS_CALLBACK_RECEIVE", &server_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING]);
            lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
            break;
 
        case LWS_CALLBACK_SERVER_WRITEABLE: //可写 //会朝所有的连接写入数据,此处至少是2个连接(发送数字的ws客户端和html里面建立的一个ws客户端),所以log可以看到2条
            printf("\033[31mserver callback: %s->%s\n", "LWS_CALLBACK_SERVER_WRITEABLE", &server_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING]);
            lws_write( wsi, &server_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], server_received_payload.len, LWS_WRITE_TEXT );
            break;
 
        default:
            break;
    }
 
    return 0;
}
 
//ws服务端在同一个端口支持2种协议,http和ws(具体是ws-protocol-example,实际上可以再多几个ws协议)
static struct lws_protocols server_protocols[] =
{
    /* The first protocol must always be the HTTP handler */
    {
        "http-only",   /* name */
        callback_http, /* callback */
        0,             /* No per session data. */
        0,             /* max frame size / rx buffer */
    },
    {
        "ws-protocol-example",
        callback_example_server,
        0,
        EXAMPLE_RX_BUFFER_BYTES,
    },
    { NULL, NULL, 0, 0 } /* terminator */
};
 
//s停止服务器
static int stop_server = 0;
int main_server()
{
    struct lws_context_creation_info info;
    memset( &info, 0, sizeof(info) );
 
    info.port = 8000;//监听端口
    info.protocols = server_protocols; //支持哪些协议
    info.gid = -1;
    info.uid = -1;
 
    struct lws_context *context = lws_create_context( &info );
    printf("websockets server starts at %d \n", info.port);
    while( stop_server == 0 )
    {
        lws_service( context, /* timeout_ms = */ 1000000 );
    }
 
    lws_context_destroy( context );
 
    return 0;
}
 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////  以下为ws客户端的内容 /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//用于保存客户端与服务端的连接
static struct lws *web_socket = NULL;
 
struct payload client_received_payload; //保存客户端收到的数据
 
static int report_count = 0; //客户端发送的数据
 
//客户端回调
static int callback_example_client( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
    switch( reason )
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED: //建立连接成功
            printf("\033[34mclient callback: %s->%s\n", "LWS_CALLBACK_CLIENT_ESTABLISHED", "");
            lws_callback_on_writable( wsi );
            break;
 
        case LWS_CALLBACK_CLIENT_RECEIVE://回显服务器发回来的数据
            memset(&client_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], 0, len + 1);
            memcpy( &client_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], in, len );
            client_received_payload.len = len;
            printf("\033[34mclient callback: %s->%s\n", "LWS_CALLBACK_CLIENT_RECEIVE", &client_received_payload.data[LWS_SEND_BUFFER_PRE_PADDING]);
            
            break;
 
        case LWS_CALLBACK_CLIENT_WRITEABLE: //每次回调,count加一,发送给服务器
        {
            unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
            unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
            size_t n = sprintf( (char *)p, "%u", report_count++ );
            printf("\033[34mclient callback: %s->%s\n", "LWS_CALLBACK_CLIENT_WRITEABLE", p);
            lws_write( wsi, p, n, LWS_WRITE_TEXT );
            break;
        }
 
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            //连接失败或者中断,置空,便于重新连接
            web_socket = NULL;
            break;
 
        default:
            break;
    }
 
    return 0;
}
 
//客户端只需要支持ws(ws-protocol-example)
enum client_protocols_type
{
    WS_PROTOCOL_EXAMPLE = 0,
    PROTOCOL_COUNT
};
static struct lws_protocols client_protocols[] =
{
    {
        "ws-protocol-example", //协议名称,一个端口可以建立多个ws协议
        callback_example_client,
        0,
        EXAMPLE_RX_BUFFER_BYTES,
    },
    { NULL, NULL, 0, 0 } /* terminator */
};
 
static int stop_client = 0;
int main_client()
{
    struct lws_context_creation_info info;
    memset( &info, 0, sizeof(info) );
 
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = client_protocols;
    info.gid = -1;
    info.uid = -1;
 
    struct lws_context *context = lws_create_context( &info );
 
    time_t old = 0;
    while( stop_client == 0  )
    {
        struct timeval tv;
        gettimeofday( &tv, NULL );
 
        /* Connect if we are not connected to the server. */
        if( !web_socket && tv.tv_sec != old )
        {
            struct lws_client_connect_info ccinfo = {0};
            ccinfo.context = context;
            ccinfo.address = "localhost";
            ccinfo.port = 8000;
            ccinfo.path = "/";
            ccinfo.host = lws_canonical_hostname( context );
            ccinfo.origin = "origin";
            ccinfo.protocol = client_protocols[WS_PROTOCOL_EXAMPLE].name;
            web_socket = lws_client_connect_via_info(&ccinfo);
        }
 
        if( tv.tv_sec != old )
        {
            /* Send a random number to the server every second. */
            lws_callback_on_writable( web_socket );
            old = tv.tv_sec;
        }
 
        lws_service( context, /* timeout_ms = */ 250 );//在V3.2之后,timeout的值已经废弃,底层自己调度;所以这个地方怎么控制频率,不清楚
    }
 
    lws_context_destroy( context );
 
    return 0;
}
 
//运行server的线程函数
void thread_func_server(void *p)
{
    main_server();
}
 
 
int main( int argc, char *argv[] )
{
    //server在work thread里面调用,客户端运行在main thread
    GThread* server_thread = g_thread_new("server_thread", (GThreadFunc)(thread_func_server), NULL);
    main_client();
    g_thread_join(server_thread); //等待,无限循环
}
