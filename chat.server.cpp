#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <atomic>
#include <string>
#include "filetransferclass.h"
#include <windows.h>
#include <richedit.h>
#include <commdlg.h>
#include <vector>
#include <mutex>

#pragma comment(lib, "Comdlg32.lib")

#pragma comment(lib, "ws2_32.lib")

struct ClientInfo
{
    SOCKET socket;
    std::string username;

};

std::vector<ClientInfo> clients;

std::mutex clientsmutex;

std::thread acceptthread;

std::atomic<int> nextclientid = 1;

SOCKET serversocket = INVALID_SOCKET;


NetworkTransfer transfer;


std::thread receiverThread;

std::atomic<bool> running = true;

HWND chatbox;
HWND messagebox;
HWND sendbutton;
HWND usersbox;
HWND filebutton;

#define ID_USERS 1000
#define ID_SEND 1001
#define ID_MESSAGE 1002
#define ID_CHAT 1003
#define ID_FILE 1004
#define CHAT_PORT 17119
#define WM_NEW_CHAT (WM_APP + 1)

void addchattext(HWND box, const char* text, bool right)
{

    SendMessageA(box, EM_SETREADONLY, false, 0);

    int length = GetWindowTextLengthA(box);

    SendMessageA(box, EM_SETSEL, length, length);

    PARAFORMAT2 format{};

    format.cbSize = sizeof(PARAFORMAT2);

    format.dwMask = PFM_ALIGNMENT;

    if (right)
    {
        format.wAlignment = PFA_RIGHT;
    }

    else
    {
        format.wAlignment = PFA_LEFT;

    }

    SendMessageA(box, EM_SETPARAFORMAT, 0, (LPARAM)&format);

    SendMessageA(box, EM_REPLACESEL, FALSE, (LPARAM)text);

    SendMessageA(box, EM_SETREADONLY, true, 0);

}

void ReceiveMessages(SOCKET client, HWND hwnd)
{
    std::string username = "unknown";

    while (running)
    {
        std::string received;

        char type = transfer.receive(client, received);

        if (type == 0)
        {
            break;
        }


        if (type == 'M')
        {

            if (received.rfind("USERNAME:", 0) == 0)
            {
                username = received.substr(9);

                {
                    std::lock_guard<std::mutex> lock(clientsmutex);

                    for (ClientInfo& c : clients)
                    {
                        if (c.socket == client)
                        {
                            c.username = username;
                            break;
                        }

                    }
                }

                std::string* text = new std::string(username + " connected\r\n");

                PostMessageA(hwnd, WM_NEW_CHAT, (WPARAM)text, 0);

                continue;

            }

            std::string* text = new std::string(username + ": " + received + "\r\n");

            PostMessageA(hwnd, WM_NEW_CHAT, (WPARAM)text, 0);
        }


        if (type == 'F')
        {
            std::string* text = new std::string(username + "sent a file\r\n");

            PostMessageA(hwnd, WM_NEW_CHAT, (WPARAM)text, 0);
        }
    }

    closesocket(client);


    std::lock_guard<std::mutex> lock(clientsmutex);


    for (auto it = clients.begin(); it != clients.end(); ++it)
    {

        if (it->socket == client)
        {
            clients.erase(it);
            break;
        }
    }
        


}


void waitforclients(HWND hwnd)
{
    while (running)
    {

        SOCKET newclient = accept(serversocket, NULL, NULL);


        if (newclient == INVALID_SOCKET)
        {
            break;

        }

        
        ClientInfo info;

        info.socket = newclient;
        info.username = "unknown";

        {
            std::lock_guard<std::mutex> lock(clientsmutex);

            clients.push_back(info);

        }

        

        std::thread(ReceiveMessages, newclient, hwnd).detach();


    }

}

bool hasclients()
{

    std::lock_guard<std::mutex> lock(clientsmutex);

    return !clients.empty();

}

bool sendmessagetoall(const char* message)
{
    std::lock_guard<std::mutex> lock(clientsmutex);


    for (ClientInfo& client : clients)
    {
        transfer.sendMessage(client.socket, message);

    }

    return true;

}

bool SendFileToAll(const char* filename)
{
    std::lock_guard<std::mutex> lock(clientsmutex);

    for (ClientInfo& client : clients)
    {

        transfer.sendFile(client.socket, filename);

    }

    return true;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {

        usersbox = CreateWindowA("STATIC", "Users: [Client]", WS_CHILD | WS_VISIBLE | WS_BORDER, 20, 20, 540, 55, hwnd, (HMENU)ID_USERS, NULL, NULL);

        chatbox = CreateWindowA("RICHEDIT20A", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 20, 85, 540, 280, hwnd, (HMENU)ID_CHAT, NULL, NULL);

        messagebox = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 100, 380, 350, 32, hwnd, (HMENU)ID_MESSAGE, NULL, NULL);

        sendbutton = CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 460, 380, 100, 32, hwnd, (HMENU)ID_SEND, NULL, NULL);

        filebutton = CreateWindowA("BUTTON", "file", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 380, 70, 32, hwnd, (HMENU)ID_FILE, NULL, NULL);

        return 0;
    }


    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_SEND)
        {
            if (!hasclients())
            {

                MessageBoxA(hwnd, "no clients connected", "chat", MB_OK);

                return 0;
            }


            char message[1024];


            GetWindowTextA(messagebox, message, sizeof(message));


            if (strlen(message) == 0)
                return 0;



            if (sendmessagetoall(message))
            {

                std::string text = "";

                text += message;
                text += "\r\n";

                addchattext(chatbox, text.c_str(), true);

            }


            SetWindowTextA(messagebox, "");
            SetFocus(messagebox);

            return 0;

        }



        if (LOWORD(wParam) == ID_FILE)
        {
            if (!hasclients())
            {

                MessageBoxA(hwnd, "noclients connected", "chat", MB_OK);

                return 0;
            }

            char filename[MAX_PATH] = "";

            OPENFILENAMEA file{};

            file.lStructSize = sizeof(OPENFILENAMEA);

            file.hwndOwner = hwnd;

            file.lpstrFile = filename;

            file.nMaxFile = MAX_PATH;

            file.lpstrFilter = "all files\0*.*\0\0";

            file.Flags = OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&file))
            {
                if (SendFileToAll(filename))
                {

                    std::string text = "You sent: ";

                    text += filename;
                    text += "\r\n";

                    addchattext(chatbox, text.c_str(), true);

                }


            }

            SetFocus(messagebox);
            return 0;
        }

        return 0;
    }



    case WM_NEW_CHAT:
    {
        std::string* text = (std::string*)wParam;

        addchattext(chatbox, text->c_str(), false);

        delete text;

        return 0;
    }


    case WM_DESTROY:
    {
        running = false;

        std::lock_guard<std::mutex> lock(clientsmutex);

        for (ClientInfo& client : clients)
        {

            shutdown(client.socket, SD_BOTH);

            closesocket(client.socket);

        }

        clients.clear();



        if (serversocket != INVALID_SOCKET)
        {

            closesocket(serversocket);

            serversocket = INVALID_SOCKET;

        }



        if (acceptthread.joinable())
        {
            acceptthread.join();
        }



        WSACleanup();

        PostQuitMessage(0);

        return 0;
    }


    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

    WSADATA wsadata;


    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {

        MessageBoxA(NULL, "WSAStartup failed", "error", MB_OK);

        return 1;
    }


    serversocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serversocket == INVALID_SOCKET)
    {

        MessageBoxA(NULL, "socket failed", "error", MB_OK);

        return 1;

    }
    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CHAT_PORT);


    if (bind(serversocket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {

        MessageBoxA(NULL, "bind failed", "error", MB_OK);

        return 1;

    }

    if (listen(serversocket, SOMAXCONN) == SOCKET_ERROR)
    {

        MessageBoxA(NULL, "listen failed", "error", MB_OK);

        return 1;

    }



    WNDCLASSA wc{};

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "WindowClass";

    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);




    if (!RegisterClassA(&wc))
    {
        MessageBoxA(NULL, "RegisterClass failed", "Error", MB_OK);

        return 1;
    }

    LoadLibraryA("Riched20.dll");

    HWND hwnd = CreateWindowA("WindowClass", "Server Chat", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 700, 470, NULL, NULL, hInstance, NULL);


    if (hwnd == NULL)
    {
        MessageBoxA(NULL, "CreateWindow failed", "Error", MB_OK);
        return 1;
    }


    ShowWindow(hwnd, nCmdShow);

    UpdateWindow(hwnd);

    acceptthread = std::thread(waitforclients, hwnd);

    MSG msg{};

    while (GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

}