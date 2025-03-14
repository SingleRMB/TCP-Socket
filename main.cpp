#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <random>
#include <sstream>
#include <map>
#include <cmath>
#include <windows.h>
#include "resource.h"

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8080"
#define BUFFER_SIZE 1024

HWND hStartButton, hStopButton, hLogEdit;
bool serverRunning = false;
SOCKET serverSocket;
std::vector<std::thread> clientThreads;

void logMessage(const std::string& message) {
    int len = GetWindowTextLength(hLogEdit);

    // ×ª»» ANSI/UTF-8 µ½ Unicode
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    std::wstring wideMessage(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wideMessage[0], wideLen);

    SendMessage(hLogEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)wideMessage.c_str());
}


std::vector<int> generateRandomNumbers(int n) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::vector<int> numbers;
    for (int i = 0; i < n; ++i) {
        numbers.push_back(dis(gen));
    }
    return numbers;
}

std::string base29Encode(int number) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123";
    std::string result;
    do {
        result = chars[number % 29] + result;
        number /= 29;
    } while (number > 0);
    return result;
}

std::pair<int, int> performDHExchange(int p) {
    int base = 5;
    int secret = rand() % (p - 1) + 1;
    int exponent = (int)pow(base, secret) % p;
    return { base, exponent };
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    send(clientSocket, "00 WELCOME C++ TCP SERVER\r\n", 27, 0);
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) break;
        buffer[bytesReceived] = '\0';
        std::istringstream iss(buffer);
        std::string command;
        iss >> command;

        if (command == "RAND") {
            int n;
            iss >> n;
            send(clientSocket, "01 OK\r\n", 8, 0);
            for (int num : generateRandomNumbers(n)) {
                std::string msg = std::to_string(num) + "\r\n";
                send(clientSocket, msg.c_str(), msg.length(), 0);
            }
            send(clientSocket, ".\r\n", 3, 0);
        }
        else if (command == "BASE29") {
            int n;
            iss >> n;
            std::string response = "01 OK " + base29Encode(n) + "\r\n";
            send(clientSocket, response.c_str(), response.length(), 0);
        }
        else if (command == "DH") {
            int p;
            iss >> p;
            std::pair<int, int> dh_result = performDHExchange(p);
            int b = dh_result.first;
            int e = dh_result.second;
            std::string response = "01 OK " + std::to_string(b) + " " + std::to_string(e) + "\r\n";
            send(clientSocket, response.c_str(), response.length(), 0);
            recv(clientSocket, buffer, BUFFER_SIZE, 0);
            int e2 = std::stoi(buffer);
            int key = (int)pow(e2, e) % p;
            response = "03 DATA " + std::to_string(key) + "\r\n";
            send(clientSocket, response.c_str(), response.length(), 0);
        }
    }
    closesocket(clientSocket);
}

void startServer() {
    WSADATA wsaData;
    struct addrinfo* result = NULL, hints;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    bind(serverSocket, result->ai_addr, (int)result->ai_addrlen);
    listen(serverSocket, SOMAXCONN);
    logMessage("Server started on port " DEFAULT_PORT "\r\n");
    while (serverRunning) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            clientThreads.emplace_back(handleClient, clientSocket);
        }
    }
    closesocket(serverSocket);
    WSACleanup();
}

void stopServer() {
    serverRunning = false;
    closesocket(serverSocket);
    logMessage("Server stopped.\r\n");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            serverRunning = true;
            std::thread(startServer).detach();
        }
        else if (LOWORD(wParam) == 2) {
            stopServer();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TCPServerClass";
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, L"TCPServerClass", L"TCP Server", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);
    hStartButton = CreateWindow(L"BUTTON", L"Start", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 50, 200, 100, 30, hwnd, (HMENU)1, hInstance, NULL);
    hStopButton = CreateWindow(L"BUTTON", L"Stop", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 200, 200, 100, 30, hwnd, (HMENU)2, hInstance, NULL);
    hLogEdit = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 20, 20, 350, 150, hwnd, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
